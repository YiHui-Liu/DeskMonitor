#include "ui/ui_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_io.h"

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

static void render_summary_page(void) {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "DeskMonitor");
  lv_obj_set_style_text_color(title, lv_color_hex(0x7EBCFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  lv_obj_t *subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "ESP32 ST7796S online");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xB8C4D0), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 52);

  lv_obj_t *body = lv_label_create(screen);
  lv_label_set_text(body, "WiFi: AP DeskMonitor-Setup\n"
                          "Web:  http://192.168.4.1/\n"
                          "API:  /api/diagnostics");
  lv_obj_set_style_text_color(body, lv_color_hex(0xE0E6EC), 0);
  lv_obj_align(body, LV_ALIGN_TOP_LEFT, 28, 96);
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

static esp_err_t init_lvgl(void) {
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

  render_summary_page();

  BaseType_t ok = xTaskCreate(lvgl_task, "deskmon_lvgl", 6 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
  return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t deskmon_display_init(void) {
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

  err = init_lvgl();
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
