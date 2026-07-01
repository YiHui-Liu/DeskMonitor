#include "ui/ui_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "bsp/bsp_io.h"
#include "ui/ui_carousel.h"
#include "ui/ui_font.h"
#include "ui/ui_pages.h"

#include "draw/sw/lv_draw_sw_utils.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "deskmon_display";

/* Partial render slices for the no-PSRAM ESP32. Buffer stride uses the larger
 * panel dimension so it fits both portrait and landscape logical orientations.
 * The flush callback waits for ESP LCD's transfer-done callback before letting
 * LVGL reuse the active buffer. */
#define DISPLAY_BUF_LINES 20
#define DISPLAY_BUF_STRIDE (DESKMON_DISPLAY_WIDTH > DESKMON_DISPLAY_HEIGHT ? DESKMON_DISPLAY_WIDTH : DESKMON_DISPLAY_HEIGHT)
#define DISPLAY_BUF_BYTES (DISPLAY_BUF_STRIDE * DISPLAY_BUF_LINES * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))

static esp_lcd_panel_handle_t s_panel;
static lv_display_t *s_display;
static SemaphoreHandle_t s_flush_done_sem;
static lv_obj_t *s_header_title;
static lv_obj_t *s_header_clock;
static lv_obj_t *s_content;
static lv_obj_t *s_footer_items[DESKMON_PAGE_COUNT];
static deskmon_page_id_t s_active_page = DESKMON_PAGE_SUMMARY;
static bool s_enabled_pages[DESKMON_PAGE_COUNT] = {true, true, true, true, true};
static uint32_t s_carousel_period_ms = 15U * 1000U;
static deskmon_display_snapshot_t s_snapshot;
static deskmon_display_snapshot_provider_t s_snapshot_provider;
static void *s_snapshot_ctx;
static LV_ATTRIBUTE_MEM_ALIGN uint8_t s_draw_buf1[DISPLAY_BUF_BYTES];
static LV_ATTRIBUTE_MEM_ALIGN uint8_t s_draw_buf2[DISPLAY_BUF_BYTES];

static uint32_t display_tick_cb(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void display_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
  /* esp_lcd_panel_draw_bitmap end coordinates are exclusive; LVGL area is inclusive. */
  const int x_start = area->x1;
  const int y_start = area->y1;
  const int x_end = area->x2 + 1;
  const int y_end = area->y2 + 1;
  const uint32_t pixel_count = (uint32_t)(x_end - x_start) * (uint32_t)(y_end - y_start);
  xSemaphoreTake(s_flush_done_sem, 0);
  lv_draw_sw_rgb565_swap(px_map, pixel_count);
  esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end, px_map);
  if (err == ESP_OK) {
    xSemaphoreTake(s_flush_done_sem, portMAX_DELAY);
  } else {
    ESP_LOGW(TAG, "LCD flush failed: %s", esp_err_to_name(err));
  }
  lv_display_flush_ready(display);
}

static bool display_flush_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                  void *user_ctx) {
  (void)panel_io;
  (void)edata;
  (void)user_ctx;
  BaseType_t task_woken = pdFALSE;
  xSemaphoreGiveFromISR(s_flush_done_sem, &task_woken);
  return task_woken == pdTRUE;
}

static void style_panel(lv_obj_t *obj, lv_color_t bg, lv_color_t border) {
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(obj, bg, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *label_at(lv_obj_t *parent, const char *text, int32_t x, int32_t y, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_font(label, &deskmon_font_14, 0);
  return label;
}

/* Header clock shows date + time with seconds. Until the system clock is set
 * (it boots at the epoch), keep the literal fallback so the header always
 * reads sensibly. Only the local RTC is read here -- no SNTP/network plumbing. */
#define HEADER_CLOCK_FALLBACK "2026-06-30 10:30:00"
/* tm_year is years since 1900; treat anything in 2020+ as a set clock. */
#define HEADER_CLOCK_MIN_YEAR (2020 - 1900)

static void header_clock_update(void) {
  if (s_header_clock == NULL) {
    return;
  }
  time_t now = time(NULL);
  struct tm tm_local;
  char buf[24] = {0};
  if (now > 0 && localtime_r(&now, &tm_local) != NULL && tm_local.tm_year >= HEADER_CLOCK_MIN_YEAR) {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_local);
    lv_label_set_text(s_header_clock, buf);
  } else {
    lv_label_set_text(s_header_clock, HEADER_CLOCK_FALLBACK);
  }
}

static void header_clock_timer_cb(lv_timer_t *timer) {
  (void)timer;
  header_clock_update();
}

static void footer_update(void) {
  static const char *const nav_labels[DESKMON_PAGE_COUNT] = {"首页", "天气", "传感", "备忘", "相册"};

  for (int i = 0; i < DESKMON_PAGE_COUNT; ++i) {
    lv_obj_t *item = s_footer_items[i];
    if (item == NULL) {
      continue;
    }
    const bool active = (i == s_active_page);
    lv_obj_set_style_bg_color(item, lv_color_hex(active ? 0xEAF3FF : 0xFFFFFF), 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_text_color(item, lv_color_hex(active ? 0x155CC9 : 0x606A78), 0);
    lv_obj_t *label = lv_obj_get_child(item, 0);
    lv_label_set_text(label, nav_labels[i]);
    lv_obj_center(label);
  }
}

static void show_page(deskmon_page_id_t page) {
  s_active_page = (page >= 0 && page < DESKMON_PAGE_COUNT) ? page : DESKMON_PAGE_SUMMARY;
  if (s_snapshot_provider != NULL) {
    s_snapshot_provider(&s_snapshot, s_snapshot_ctx);
  }
  lv_label_set_text(s_header_title, deskmon_page_title(s_active_page));
  lv_obj_clean(s_content);
  deskmon_page_create(s_active_page, s_content, &s_snapshot);
  footer_update();
}

static void display_data_timer_cb(lv_timer_t *timer) {
  (void)timer;
  show_page(s_active_page);
}

static void carousel_timer_cb(lv_timer_t *timer) {
  (void)timer;
  deskmon_page_id_t next = deskmon_carousel_next_enabled(s_active_page, s_enabled_pages);
  if (next != s_active_page) {
    show_page(next);
  }
}

static void apply_display_config(const deskmon_display_config_t *config) {
  if (config == NULL) {
    return;
  }
  for (int i = 0; i < DESKMON_PAGE_COUNT; ++i) {
    s_enabled_pages[i] = config->enabled_pages[i];
  }
  s_carousel_period_ms = config->carousel_interval_sec * 1000U;
  s_snapshot_provider = config->snapshot_provider;
  s_snapshot_ctx = config->snapshot_ctx;
}

static void render_dashboard(const deskmon_display_config_t *config) {
  apply_display_config(config);
  s_active_page = deskmon_carousel_next_enabled(DESKMON_PAGE_ALBUM, s_enabled_pages);

  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xF6F9FC), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(screen, &deskmon_font_14, 0);
  deskmon_display_snapshot_defaults(&s_snapshot);

  lv_obj_t *header = lv_obj_create(screen);
  style_panel(header, lv_color_hex(0xFFFFFF), lv_color_hex(0xE5EAF0));
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_size(header, 480, 38);
  s_header_title = label_at(header, "", 0, 7, lv_color_hex(0x111827));
  lv_obj_set_width(s_header_title, 480);
  lv_obj_set_style_text_align(s_header_title, LV_TEXT_ALIGN_CENTER, 0);
  s_header_clock = label_at(header, HEADER_CLOCK_FALLBACK, 8, 7, lv_color_hex(0x111827));
  label_at(header, "WiFi", 368, 7, lv_color_hex(0x111827));
  label_at(header, "在线", 412, 7, lv_color_hex(0x21B650));

  s_content = lv_obj_create(screen);
  lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(s_content, 0, 38);
  lv_obj_set_size(s_content, 480, 236);
  lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_content, 0, 0);
  lv_obj_set_style_pad_all(s_content, 0, 0);

  lv_obj_t *footer = lv_obj_create(screen);
  style_panel(footer, lv_color_hex(0xFFFFFF), lv_color_hex(0xE5EAF0));
  lv_obj_set_pos(footer, 0, 274);
  lv_obj_set_size(footer, 480, 46);
  for (int i = 0; i < DESKMON_PAGE_COUNT; ++i) {
    lv_obj_t *item = lv_obj_create(footer);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(item, 10 + i * 92, 6);
    lv_obj_set_size(item, 84, 34);
    lv_obj_set_style_pad_all(item, 0, 0);
    s_footer_items[i] = item;
    lv_obj_t *label = lv_label_create(item);
    lv_obj_center(label);
  }

  show_page(s_active_page);
  header_clock_update();
  lv_timer_t *clock_timer = lv_timer_create(header_clock_timer_cb, 1000, NULL);
  if (clock_timer == NULL) {
    ESP_LOGW(TAG, "header clock timer alloc failed; clock will be static");
  }
  lv_timer_t *data_timer = lv_timer_create(display_data_timer_cb, 30U * 1000U, NULL);
  if (data_timer == NULL) {
    ESP_LOGW(TAG, "display data timer alloc failed; pages refresh only on carousel");
  }
  lv_timer_t *timer = lv_timer_create(carousel_timer_cb, s_carousel_period_ms, NULL);
  if (timer == NULL) {
    ESP_LOGW(TAG, "carousel timer alloc failed; display will be static");
  }
}

static void lvgl_task(void *arg) {
  (void)arg;
  while (true) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static esp_err_t init_backlight(void) {
  gpio_config_t bl_conf = {
      .pin_bit_mask = (1ULL << DESKMON_DISPLAY_BL_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t err = gpio_config(&bl_conf);
  if (err != ESP_OK) {
    return err;
  }
  return gpio_set_level(DESKMON_DISPLAY_BL_GPIO, 1);
}

static esp_err_t init_spi_panel(void) {
  esp_lcd_panel_io_handle_t io_handle = NULL;
  bool spi_ready = false;

  s_flush_done_sem = xSemaphoreCreateBinary();
  if (s_flush_done_sem == NULL) {
    return ESP_ERR_NO_MEM;
  }

  const size_t max_transfer = DISPLAY_BUF_BYTES;
  const spi_bus_config_t bus_cfg = {
      .sclk_io_num = DESKMON_DISPLAY_SCLK_GPIO,
      .mosi_io_num = DESKMON_DISPLAY_MOSI_GPIO,
      .miso_io_num = DESKMON_DISPLAY_MISO_GPIO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = (int)max_transfer,
  };
  esp_err_t err = spi_bus_initialize(DESKMON_DISPLAY_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    goto fail;
  }
  spi_ready = true;

  esp_lcd_panel_io_spi_config_t io_cfg = {
      .cs_gpio_num = DESKMON_DISPLAY_CS_GPIO,
      .dc_gpio_num = DESKMON_DISPLAY_DC_GPIO,
      .spi_mode = DESKMON_DISPLAY_SPI_MODE,
      .pclk_hz = DESKMON_DISPLAY_PCLK_HZ,
      .trans_queue_depth = DESKMON_DISPLAY_TRANS_QUEUE_DEPTH,
      .on_color_trans_done = display_flush_done_cb,
      .user_ctx = NULL,
      .lcd_cmd_bits = DESKMON_DISPLAY_CMD_BITS,
      .lcd_param_bits = DESKMON_DISPLAY_PARAM_BITS,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .flags =
          {
              .dc_high_on_cmd = 0,
              .dc_low_on_data = 0,
              .dc_low_on_param = 0,
              .octal_mode = 0,
              .quad_mode = 0,
              .sio_mode = 0,
              .lsb_first = 0,
              .cs_high_active = 0,
          },
  };
  err = esp_lcd_new_panel_io_spi(DESKMON_DISPLAY_SPI_HOST, &io_cfg, &io_handle);
  if (err != ESP_OK) {
    goto fail;
  }

  const esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = DESKMON_DISPLAY_RST_GPIO,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
      .bits_per_pixel = 16,
      .vendor_config = NULL,
      .flags =
          {
              .reset_active_high = 0,
          },
  };
  err = esp_lcd_new_panel_st7796(io_handle, &panel_cfg, &s_panel);
  if (err != ESP_OK) {
    goto fail;
  }

  err = esp_lcd_panel_reset(s_panel);
  if (err != ESP_OK) {
    goto fail;
  }
  err = esp_lcd_panel_init(s_panel);
  if (err != ESP_OK) {
    goto fail;
  }
#if DESKMON_DISPLAY_SWAP_XY
  err = esp_lcd_panel_swap_xy(s_panel, true);
  if (err != ESP_OK) {
    goto fail;
  }
#endif
  err = esp_lcd_panel_mirror(s_panel, DESKMON_DISPLAY_MIRROR_X, DESKMON_DISPLAY_MIRROR_Y);
  if (err != ESP_OK) {
    goto fail;
  }
  err = esp_lcd_panel_disp_on_off(s_panel, true);
  if (err != ESP_OK) {
    goto fail;
  }
  return ESP_OK;

fail:
  if (s_panel != NULL) {
    esp_lcd_panel_del(s_panel);
    s_panel = NULL;
  }
  if (io_handle != NULL) {
    esp_lcd_panel_io_del(io_handle);
  }
  if (spi_ready) {
    spi_bus_free(DESKMON_DISPLAY_SPI_HOST);
  }
  if (s_flush_done_sem != NULL) {
    vSemaphoreDelete(s_flush_done_sem);
    s_flush_done_sem = NULL;
  }
  return err;
}

static esp_err_t init_lvgl(const deskmon_display_config_t *config) {
  lv_init();
  lv_tick_set_cb(display_tick_cb);

  const int32_t hor_res = DESKMON_DISPLAY_SWAP_XY ? DESKMON_DISPLAY_HEIGHT : DESKMON_DISPLAY_WIDTH;
  const int32_t ver_res = DESKMON_DISPLAY_SWAP_XY ? DESKMON_DISPLAY_WIDTH : DESKMON_DISPLAY_HEIGHT;
  s_display = lv_display_create(hor_res, ver_res);
  if (s_display == NULL) {
    return ESP_FAIL;
  }
  lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(s_display, s_draw_buf1, s_draw_buf2, DISPLAY_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(s_display, display_flush_cb);

  render_dashboard(config);

  BaseType_t ok = xTaskCreate(lvgl_task, "deskmon_lvgl", 6 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
  return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t deskmon_display_init(const deskmon_display_config_t *config) {
#if !DESKMON_DISPLAY_ENABLED
  ESP_LOGW(TAG, "display disabled in bsp_io.h");
  return ESP_ERR_NOT_SUPPORTED;
#else
  esp_err_t err = init_backlight();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "backlight init failed: %s", esp_err_to_name(err));
    return err;
  }

  err = init_spi_panel();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "SPI/ST7796 init failed: %s", esp_err_to_name(err));
    gpio_set_level(DESKMON_DISPLAY_BL_GPIO, 0);
    return err;
  }

  err = init_lvgl(config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "LVGL init failed: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "display up: %dx%d ST7796S MOSI=%d SCLK=%d CS=%d DC=%d BL=%d", DESKMON_DISPLAY_WIDTH,
           DESKMON_DISPLAY_HEIGHT, DESKMON_DISPLAY_MOSI_GPIO, DESKMON_DISPLAY_SCLK_GPIO, DESKMON_DISPLAY_CS_GPIO,
           DESKMON_DISPLAY_DC_GPIO, DESKMON_DISPLAY_BL_GPIO);
  return ESP_OK;
#endif
}

bool deskmon_display_is_enabled(void) { return DESKMON_DISPLAY_ENABLED != 0; }
