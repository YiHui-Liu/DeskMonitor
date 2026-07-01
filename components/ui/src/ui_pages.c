#include "ui/ui_font.h"
#include "ui/ui_pages.h"

static void style_plain(lv_obj_t *obj) {
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *card_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFCFEFF), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xD7E4F2), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, 6, 0);
  return card;
}

static lv_obj_t *label_create(lv_obj_t *parent, const char *text, int32_t x, int32_t y, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_font(label, &deskmon_font_14, 0);
  return label;
}

static void label_center(lv_obj_t *parent, const char *text, int32_t y, lv_color_t color) {
  lv_obj_t *label = label_create(parent, text, 0, y, color);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

/* ---- Sensor page sparkline data (8 points each, x: 0-140 step 20, y: 0-40 inverted) ---- */

static const lv_point_precise_t spark_temp[] = {
    {0, 30}, {20, 25}, {40, 18}, {60, 8}, {80, 5}, {100, 12}, {120, 22}, {140, 28}};
static const lv_point_precise_t spark_humid[] = {
    {0, 10}, {20, 15}, {40, 22}, {60, 30}, {80, 35}, {100, 28}, {120, 18}, {140, 12}};
static const lv_point_precise_t spark_light[] = {
    {0, 38}, {20, 35}, {40, 25}, {60, 12}, {80, 5}, {100, 15}, {120, 30}, {140, 38}};
static const lv_point_precise_t spark_co2[] = {
    {0, 25}, {20, 20}, {40, 15}, {60, 8}, {80, 5}, {100, 10}, {120, 18}, {140, 22}};
static const lv_point_precise_t spark_tvoc[] = {
    {0, 30}, {20, 28}, {40, 20}, {60, 5}, {80, 15}, {100, 25}, {120, 22}, {140, 28}};
static const lv_point_precise_t spark_aqi[] = {
    {0, 20}, {20, 18}, {40, 22}, {60, 15}, {80, 25}, {100, 20}, {120, 18}, {140, 22}};

static void sensor_pad(lv_obj_t *parent, int32_t x, int32_t y, const char *icon, const char *name,
                       const char *reading, const char *status, const char *stats, lv_color_t accent,
                       const lv_point_precise_t *pts, uint16_t cnt) {
  lv_obj_t *card = card_create(parent, x, y, 153, 113);
  label_create(card, icon, 0, 0, accent);
  label_create(card, name, 20, 1, lv_color_hex(0x1F2937));
  label_create(card, reading, 88, 1, lv_color_hex(0x111827));
  label_create(card, status, 0, 20, accent);
  label_create(card, stats, 0, 40, lv_color_hex(0x4B5563));

  lv_obj_t *line = lv_line_create(card);
  lv_line_set_points(line, pts, cnt);
  lv_obj_set_pos(line, 0, 58);
  lv_obj_set_style_line_color(line, accent, 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

static void render_summary(lv_obj_t *parent) {
  lv_obj_t *date = card_create(parent, 8, 6, 172, 72);
  label_create(date, "日", 10, 18, lv_color_hex(0x256BBD));
  label_create(date, "2026-06-30\n星期二\n10:30", 54, 8, lv_color_hex(0x111827));

  lv_obj_t *weather = card_create(parent, 188, 6, 284, 72);
  label_create(weather, "晴", 18, 14, lv_color_hex(0xF5B800));
  label_create(weather, "28°C\n多云", 64, 10, lv_color_hex(0x111827));
  label_create(weather, "湿度 62%\n风速 2.6m/s", 198, 14, lv_color_hex(0x1F2937));

  lv_obj_t *memo = card_create(parent, 8, 86, 464, 46);
  label_create(memo, "备忘录摘要", 16, 10, lv_color_hex(0x155CC9));
  label_create(memo, "· 15:00 设备巡检    · 17:30 更换滤网    · 20:00 关闭新风", 112, 12, lv_color_hex(0x111827));

  label_create(parent, "传感器摘要", 14, 138, lv_color_hex(0x155CC9));
  label_create(parent, "温度 27.4°C  湿度 62%  光照 512lx", 14, 158, lv_color_hex(0x1F2937));
  label_create(parent, "CO2 724ppm  TVOC 0.38  AQI 32", 14, 176, lv_color_hex(0x1F2937));
  label_create(parent, "详细数据见传感页", 14, 200, lv_color_hex(0x606A78));
}

static void render_weather(lv_obj_t *parent) {
  lv_obj_t *main = card_create(parent, 8, 8, 220, 94);
  label_create(main, "深圳市 南山区", 12, 6, lv_color_hex(0x155CC9));
  label_create(main, "晴", 18, 34, lv_color_hex(0xF5B800));
  label_create(main, "28°C", 54, 30, lv_color_hex(0x111827));
  label_create(main, "多云", 114, 42, lv_color_hex(0x111827));

  lv_obj_t *detail = card_create(parent, 236, 8, 236, 94);
  label_create(detail, "最高 32°C     风速 东南风 2级", 12, 10, lv_color_hex(0x111827));
  label_create(detail, "最低 24°C     气压 1008 hPa", 12, 38, lv_color_hex(0x111827));
  label_create(detail, "体感 31°C     降水概率 20%", 12, 66, lv_color_hex(0x111827));

  lv_obj_t *hours = card_create(parent, 8, 110, 464, 62);
  label_create(hours, "逐小时预报", 10, 4, lv_color_hex(0x155CC9));
  label_create(hours, "现在 28°   11:00 29°   12:00 30°   13:00 31°   14:00 32°   15:00 31°", 16, 30,
               lv_color_hex(0x111827));

  lv_obj_t *days = card_create(parent, 8, 180, 464, 54);
  label_create(days, "5日预报", 10, 4, lv_color_hex(0x155CC9));
  label_create(days, "6月30 晴 24~33°C    7月1 多云 25~34°C    7月2 小雨 24~30°C", 16, 28,
               lv_color_hex(0x111827));
}

static void render_sensor(lv_obj_t *parent) {
  sensor_pad(parent, 4, 4, "温", "温度", "27.4°C", "舒适", "大29 小24 均27", lv_color_hex(0xEF4444),
             spark_temp, 8);
  sensor_pad(parent, 163, 4, "湿", "湿度", "62%", "舒适", "大70 小50 均60", lv_color_hex(0x228BE6),
             spark_humid, 8);
  sensor_pad(parent, 322, 4, "光", "光照", "512lx", "适中", "大1800 小0 均760", lv_color_hex(0xF59E0B),
             spark_light, 8);
  sensor_pad(parent, 4, 123, "C", "CO2", "724ppm", "良好", "大900 小520 均680", lv_color_hex(0x22A652),
             spark_co2, 8);
  sensor_pad(parent, 163, 123, "气", "TVOC", "0.38", "优", "大0.50 小0.15 均0.33", lv_color_hex(0x7C5CC4),
             spark_tvoc, 8);
  sensor_pad(parent, 322, 123, "空", "AQI", "32", "优", "大42 小21 均31", lv_color_hex(0x2D9CA3),
             spark_aqi, 8);
}

static void render_memo(lv_obj_t *parent) {
  lv_obj_t *hero = card_create(parent, 8, 8, 464, 90);
  label_create(hero, "记", 28, 24, lv_color_hex(0x256BBD));
  label_create(hero, "15:00 设备巡检", 64, 16, lv_color_hex(0x111827));
  label_create(hero, "检查各区域传感器状态，确认温湿度数据正常，确保通风系统运行良好。", 64, 50, lv_color_hex(0x1F2937));

  const char *rows[] = {"15:00   设备巡检                 2026-06-30", "17:30   更换滤网                 2026-06-30",
                        "20:00   关闭新风                 2026-06-30", "周末采购清单                   2026-06-28"};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *row = card_create(parent, 8, 108 + i * 32, 464, 26);
    if (i == 0) {
      lv_obj_set_style_bg_color(row, lv_color_hex(0xEEF6FF), 0);
    }
    label_create(row, rows[i], 8, 4, i == 0 ? lv_color_hex(0x155CC9) : lv_color_hex(0x111827));
  }
}

static void render_album(lv_obj_t *parent) {
  lv_obj_t *photo = card_create(parent, 8, 8, 464, 156);
  lv_obj_set_style_bg_color(photo, lv_color_hex(0xBFD3EA), 0);
  label_center(photo, "相册预览", 52, lv_color_hex(0x1F2937));
  label_center(photo, "TF 相册运行时保留，当前显示静态占位", 82, lv_color_hex(0x374151));

  lv_obj_t *info = card_create(parent, 8, 172, 464, 62);
  label_create(info, "2026-06-30 18:30    湖畔日落", 16, 10, lv_color_hex(0x111827));
  label_create(info, "第 3 张 / 共 28 张      缩略图  [1] [2] [3]", 16, 36, lv_color_hex(0x155CC9));
}

lv_obj_t *deskmon_page_create(deskmon_page_id_t page, lv_obj_t *parent) {
  deskmon_page_id_t safe_page = (page >= 0 && page < DESKMON_PAGE_COUNT) ? page : DESKMON_PAGE_SUMMARY;
  lv_obj_t *container = lv_obj_create(parent);
  style_plain(container);
  lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

  switch (safe_page) {
  case DESKMON_PAGE_WEATHER:
    render_weather(container);
    break;
  case DESKMON_PAGE_SENSOR:
    render_sensor(container);
    break;
  case DESKMON_PAGE_MEMO:
    render_memo(container);
    break;
  case DESKMON_PAGE_ALBUM:
    render_album(container);
    break;
  case DESKMON_PAGE_SUMMARY:
  default:
    render_summary(container);
    break;
  }

  return container;
}
