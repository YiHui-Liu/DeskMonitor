#include "ui/ui_pages.h"
#include "ui/ui_font.h"
#include "ui/ui_history_points.h"

#include <stdio.h>
#include <string.h>

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

/* ---- Sensor page sparkline rendering (x: 0-140, y: 0-40 inverted) ---- */

/* Vector icon + tile helpers. Icons are drawn from LVGL primitives so they
 * never depend on a glyph that may be absent from the embedded font. */

static lv_obj_t *icon_rect(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(r, x, y);
  lv_obj_set_size(r, w, h);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(r, color, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_radius(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  return r;
}

static lv_obj_t *icon_fill(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color,
                           int32_t radius) {
  lv_obj_t *r = icon_rect(parent, x, y, w, h, color);
  lv_obj_set_style_bg_color(r, color, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_radius(r, radius, 0);
  return r;
}

/* Calendar: outlined body, filled header band, two binder tabs. */
static void draw_calendar_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  icon_rect(parent, x, y + 4, 14, 11, color);
  icon_fill(parent, x, y + 4, 14, 3, color, 0);
  icon_fill(parent, x + 3, y, 2, 5, color, 0);
  icon_fill(parent, x + 9, y, 2, 5, color, 0);
}

static void draw_weather_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  const int32_t cx = x + 8;
  const int32_t cy = y + 8;
  icon_fill(parent, cx - 4, cy - 4, 8, 8, color, 4);
  static const int32_t rays[8][2] = {{7, 0}, {5, 5}, {0, 7}, {-5, 5}, {-7, 0}, {-5, -5}, {0, -7}, {5, -5}};
  for (int i = 0; i < 8; ++i) {
    icon_fill(parent, cx + rays[i][0] - 1, cy + rays[i][1] - 1, 2, 2, color, 0);
  }
}

static void draw_weather_symbol(lv_obj_t *parent, int32_t x, int32_t y, deskmon_weather_icon_t icon, float scale) {
  if (icon == DESKMON_WEATHER_ICON_SUN) {
    const int32_t cx = x + 8 * scale;
    const int32_t cy = y + 8 * scale;
    icon_fill(parent, cx - 4 * scale, cy - 4 * scale, 8 * scale, 8 * scale, lv_color_hex(0xF5B800), 4 * scale);
    static const int32_t rays[8][2] = {{7, 0}, {5, 5}, {0, 7}, {-5, 5}, {-7, 0}, {-5, -5}, {0, -7}, {5, -5}};
    for (int i = 0; i < 8; ++i) {
      icon_fill(parent, cx + rays[i][0] * scale - scale, cy + rays[i][1] * scale - scale, 2 * scale, 2 * scale,
                lv_color_hex(0xF5B800), 0);
    }
    return;
  }

  icon_fill(parent, x + 1 * scale, y + 8 * scale, 14 * scale, 8 * scale, lv_color_hex(0xB7C8E0), 4 * scale);
  icon_fill(parent, x + 5 * scale, y + 4 * scale, 12 * scale, 9 * scale, lv_color_hex(0xD7E4F2), 5 * scale);
  if (icon == DESKMON_WEATHER_ICON_RAIN) {
    icon_fill(parent, x + 5 * scale, y + 17 * scale, scale, 4 * scale, lv_color_hex(0x228BE6), 0);
    icon_fill(parent, x + 11 * scale, y + 17 * scale, scale, 4 * scale, lv_color_hex(0x228BE6), 0);
  }
}

static void draw_pressure_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  icon_rect(parent, x + 2, y + 3, 12, 10, color);
  icon_fill(parent, x + 5, y + 1, 6, 3, color, 2);
  icon_fill(parent, x + 5, y + 8, 6, 1, color, 0);
}

/* Precipitation: umbrella canopy + J-handle, distinct from the humidity droplet. */
static void draw_precip_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  icon_fill(parent, x + 1, y + 5, 14, 4, color, 7);
  icon_fill(parent, x + 7, y + 8, 2, 5, color, 0);
  icon_fill(parent, x + 5, y + 12, 4, 2, color, 1);
}

/* lv_line stores a pointer to the point array, so it must live at file scope
 * (a local array would dangle after the function returns). */
static void draw_humidity_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  static const lv_point_precise_t drop[] = {{6, 0}, {1, 7}, {0, 11}, {12, 11}, {11, 7}, {6, 0}};
  lv_obj_t *line = lv_line_create(parent);
  lv_line_set_points(line, drop, 6);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

/* Wind: three stacked swooshes; point arrays are file-scope for the same
 * lifetime reason as draw_humidity_icon. */
static void draw_wind_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  static const lv_point_precise_t sw1[] = {{0, 2}, {8, 2}, {10, 4}, {10, 8}};
  static const lv_point_precise_t sw2[] = {{2, 7}, {10, 7}, {12, 9}};
  static const lv_point_precise_t sw3[] = {{0, 12}, {6, 12}, {8, 14}};
  const lv_point_precise_t *swirls[] = {sw1, sw2, sw3};
  const uint32_t counts[] = {4, 3, 3};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, swirls[i], counts[i]);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  }
}

static void draw_temperature_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  icon_rect(parent, x + 5, y, 4, 13, color);
  icon_fill(parent, x + 3, y + 10, 8, 8, color, 4);
  icon_fill(parent, x + 6, y + 3, 2, 10, color, 1);
}

static void draw_light_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  draw_weather_icon(parent, x, y, color);
}

static void draw_co2_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  icon_fill(parent, x, y + 7, 17, 9, color, 4);
  icon_fill(parent, x + 4, y + 3, 10, 9, color, 5);
  label_create(parent, "CO2", x + 2, y + 5, lv_color_hex(0xFFFFFF));
}

static void draw_tvoc_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  icon_fill(parent, x + 6, y, 6, 6, color, 3);
  icon_fill(parent, x, y + 11, 6, 6, color, 3);
  icon_fill(parent, x + 12, y + 11, 6, 6, color, 3);
  static const lv_point_precise_t links[] = {{9, 5}, {4, 12}, {9, 5}, {15, 12}};
  lv_obj_t *line = lv_line_create(parent);
  lv_line_set_points(line, links, 4);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_width(line, 1, 0);
}

static void draw_aqi_icon(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  for (int i = 0; i < 8; ++i) {
    const int32_t px = x + 7 + ((i % 4) * 4);
    const int32_t py = y + 2 + ((i / 4) * 8);
    icon_fill(parent, px, py, 3, 3, color, 2);
  }
}

typedef void (*sensor_pad_icon_t)(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color);

static sensor_pad_icon_t sensor_icon_drawer(deskmon_sensor_icon_t icon) {
  switch (icon) {
  case DESKMON_SENSOR_ICON_TEMPERATURE:
    return draw_temperature_icon;
  case DESKMON_SENSOR_ICON_HUMIDITY:
    return draw_humidity_icon;
  case DESKMON_SENSOR_ICON_LIGHT:
    return draw_light_icon;
  case DESKMON_SENSOR_ICON_CO2:
    return draw_co2_icon;
  case DESKMON_SENSOR_ICON_TVOC:
    return draw_tvoc_icon;
  case DESKMON_SENSOR_ICON_AQI:
  default:
    return draw_aqi_icon;
  }
}

/* Horizontal + vertical axes for the 140x40 sparkline chart. */
static const lv_point_precise_t chart_axis_pts[] = {{0, 0}, {0, 40}, {140, 40}};
static void draw_chart_axes(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color) {
  lv_obj_t *axis = lv_line_create(parent);
  lv_line_set_points(axis, chart_axis_pts, 3);
  lv_obj_set_pos(axis, x, y);
  lv_obj_set_style_line_color(axis, color, 0);
  lv_obj_set_style_line_width(axis, 1, 0);
  icon_fill(parent, x, y + 13, 3, 1, color, 0);
  icon_fill(parent, x, y + 26, 3, 1, color, 0);
  icon_fill(parent, x + 46, y + 39, 1, 3, color, 0);
  icon_fill(parent, x + 93, y + 39, 1, 3, color, 0);
}

static void sensor_summary_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                const deskmon_display_sensor_t *sensor, lv_color_t accent) {
  lv_obj_t *card = card_create(parent, x, y, w, h);
  icon_fill(card, 0, 0, 4, h, accent, 0);
  sensor_icon_drawer(sensor->icon)(card, 12, 8, accent);
  label_create(card, sensor->name, 34, 6, lv_color_hex(0x6B7280));
  label_create(card, sensor->reading, 12, h / 2 - 5, lv_color_hex(0x111827));
  label_create(card, sensor->status, 12, h - 18, sensor->valid ? accent : lv_color_hex(0x9CA3AF));
}

/* cond: 0 = sun, 1 = cloud, 2 = rain. */
static void weather_day_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                             const deskmon_display_daily_t *daily, lv_color_t accent) {
  lv_obj_t *card = card_create(parent, x, y, w, h);
  char title[24];
  snprintf(title, sizeof(title), "%s", daily->date);
  label_center(card, title, 2, accent);
  draw_weather_symbol(card, 12, 30, daily->icon, 1.5);
  label_create(card, daily->low, 42, 28, lv_color_hex(0x4B5563));
  label_create(card, daily->high, 42, 48, lv_color_hex(0x111827));
}

static void weather_detail_item(lv_obj_t *parent, int32_t x, int32_t y, sensor_pad_icon_t icon, const char *label,
                                const char *value, lv_color_t icon_color, lv_color_t text_color) {
  icon(parent, x, y + 1, icon_color);
  label_create(parent, label, x + 22, y, lv_color_hex(0x4B5563));
  label_create(parent, value, x + 68, y, text_color);
}

static lv_point_precise_t s_sensor_points[DESKMON_DISPLAY_SENSOR_COUNT][DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT];

static void sensor_pad(lv_obj_t *parent, int32_t x, int32_t y, const deskmon_display_sensor_t *sensor,
                       lv_color_t accent, lv_point_precise_t *pts) {
  lv_obj_t *card = card_create(parent, x, y, 153, 113);
  sensor_icon_drawer(sensor->icon)(card, 0, 0, accent);
  label_create(card, sensor->name, 22, 1, lv_color_hex(0x1F2937));
  label_create(card, sensor->reading, 60, 1, lv_color_hex(0x111827));
  label_create(card, sensor->status, 110, 1, sensor->valid ? accent : lv_color_hex(0x9CA3AF));
  label_create(card, sensor->stats, 0, 28, lv_color_hex(0x4B5563));

  draw_chart_axes(card, 0, 62, lv_color_hex(0xC4D2E3));
  deskmon_history_point_t history_points[DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT];
  const uint32_t point_count = deskmon_history_points(sensor->samples, sensor->sample_count, 140, 40, history_points);
  if (point_count < 2) {
    return;
  }
  for (uint32_t i = 0; i < point_count; ++i) {
    pts[i].x = history_points[i].x;
    pts[i].y = history_points[i].y;
  }
  lv_obj_t *line = lv_line_create(card);
  lv_line_set_points(line, pts, point_count);
  lv_obj_set_pos(line, 0, 62);
  lv_obj_set_style_line_color(line, accent, 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

static void render_summary(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  const lv_color_t weather_text_col = snapshot->weather_valid ? lv_color_hex(0x111827) : lv_color_hex(0x9CA3AF);

  lv_obj_t *date = card_create(parent, 8, 6, 172, 72);
  draw_calendar_icon(date, 12, 18, lv_color_hex(0x256BBD));
  char date_text[40];
  snprintf(date_text, sizeof(date_text), "%s\n%s\n%s", snapshot->date, snapshot->weekday, snapshot->time);
  label_create(date, date_text, 40, 8, lv_color_hex(0x111827));

  lv_obj_t *weather = card_create(parent, 188, 6, 284, 72);
  draw_weather_icon(weather, 14, 14, lv_color_hex(0xF5B800));
  char weather_text[32];
  snprintf(weather_text, sizeof(weather_text), "%s\n%s", snapshot->weather_temperature, snapshot->weather_text);
  label_create(weather, weather_text, 40, 10, weather_text_col);
  draw_humidity_icon(weather, 150, 8, lv_color_hex(0x228BE6));
  char humidity_text[24];
  snprintf(humidity_text, sizeof(humidity_text), "湿度 %s", snapshot->weather_humidity);
  label_create(weather, humidity_text, 170, 10, weather_text_col);
  draw_wind_icon(weather, 150, 40, lv_color_hex(0x228BE6));
  label_create(weather, snapshot->wind, 170, 42, weather_text_col);

  card_create(parent, 8, 86, 464, 46);

  sensor_summary_card(parent, 8, 142, 110, 88, &snapshot->sensors[0], lv_color_hex(0xEF4444));
  sensor_summary_card(parent, 126, 142, 110, 88, &snapshot->sensors[1], lv_color_hex(0x228BE6));
  sensor_summary_card(parent, 244, 142, 110, 88, &snapshot->sensors[2], lv_color_hex(0xF59E0B));
  sensor_summary_card(parent, 362, 142, 110, 88, &snapshot->sensors[3], lv_color_hex(0x22A652));
}

static void hourly_weather_row(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot, lv_color_t text_col) {
  lv_obj_t *hours = card_create(parent, 8, 96, 464, 58);
  for (int i = 0; i < DESKMON_DISPLAY_HOURLY_COUNT; ++i) {
    label_create(hours, snapshot->hourly[i].time, 15 + i * 74, 6, lv_color_hex(0x155CC9));
    label_create(hours, snapshot->hourly[i].weather, 15 + i * 74, 30, text_col);
  }
}

static void render_weather(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  const lv_color_t text_col = snapshot->weather_valid ? lv_color_hex(0x111827) : lv_color_hex(0x9CA3AF);

  lv_obj_t *main = card_create(parent, 8, 6, 190, 82);
  label_create(main, snapshot->location, 12, 6, lv_color_hex(0x155CC9));
  draw_weather_symbol(main, 18, 30, snapshot->daily[0].icon, 2);
  label_create(main, snapshot->weather_temperature, 70, 24, text_col);
  label_create(main, snapshot->weather_text, 135, 34, text_col);
  label_create(main, "体感", 80, 56, text_col);
  label_create(main, snapshot->feels_like, 120, 56, text_col);

  lv_obj_t *detail = card_create(parent, 206, 6, 266, 82);
  weather_detail_item(detail, 10, 6, draw_temperature_icon, "最高", snapshot->high, lv_color_hex(0xEF4444), text_col);
  weather_detail_item(detail, 10, 30, draw_temperature_icon, "最低", snapshot->low, lv_color_hex(0x228BE6), text_col);
  weather_detail_item(detail, 10, 54, draw_humidity_icon, "湿度", snapshot->weather_humidity, lv_color_hex(0x228BE6),
                      text_col);
  weather_detail_item(detail, 120, 6, draw_wind_icon, "风速", snapshot->wind, lv_color_hex(0x228BE6), text_col);
  weather_detail_item(detail, 120, 30, draw_pressure_icon, "气压", snapshot->pressure, lv_color_hex(0x22A652),
                      text_col);
  weather_detail_item(detail, 120, 54, draw_precip_icon, "降水", snapshot->precip, lv_color_hex(0x228BE6), text_col);

  hourly_weather_row(parent, snapshot, text_col);

  for (int i = 0; i < DESKMON_DISPLAY_DAILY_COUNT; ++i) {
    weather_day_card(parent, 8 + i * 94, 164, 88, 70, &snapshot->daily[i], lv_color_hex(0x155CC9));
  }
}

static void render_sensor(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  sensor_pad(parent, 4, 4, &snapshot->sensors[0], lv_color_hex(0xEF4444), s_sensor_points[0]);
  sensor_pad(parent, 163, 4, &snapshot->sensors[1], lv_color_hex(0x228BE6), s_sensor_points[1]);
  sensor_pad(parent, 322, 4, &snapshot->sensors[2], lv_color_hex(0xF59E0B), s_sensor_points[2]);
  sensor_pad(parent, 4, 123, &snapshot->sensors[3], lv_color_hex(0x22A652), s_sensor_points[3]);
  sensor_pad(parent, 163, 123, &snapshot->sensors[4], lv_color_hex(0x7C5CC4), s_sensor_points[4]);
  sensor_pad(parent, 322, 123, &snapshot->sensors[5], lv_color_hex(0x2D9CA3), s_sensor_points[5]);
}

static void render_memo(lv_obj_t *parent) {
  lv_obj_t *hero = card_create(parent, 8, 8, 464, 90);
  label_create(hero, "记", 28, 24, lv_color_hex(0x256BBD));
  label_create(hero, "15:00 设备巡检", 64, 16, lv_color_hex(0x111827));
  label_create(hero, "检查各区域传感器状态，确认温湿度数据正常，确保通风系统运行良好。", 64, 50,
               lv_color_hex(0x1F2937));

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

void deskmon_display_snapshot_init(deskmon_display_snapshot_t *snapshot) {
  if (snapshot == NULL) {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));

  const deskmon_display_sensor_t sensors[DESKMON_DISPLAY_SENSOR_COUNT] = {
      {.name = "温度", .icon = DESKMON_SENSOR_ICON_TEMPERATURE}, {.name = "湿度", .icon = DESKMON_SENSOR_ICON_HUMIDITY},
      {.name = "光照", .icon = DESKMON_SENSOR_ICON_LIGHT},       {.name = "CO2", .icon = DESKMON_SENSOR_ICON_CO2},
      {.name = "TVOC", .icon = DESKMON_SENSOR_ICON_TVOC},        {.name = "AQI", .icon = DESKMON_SENSOR_ICON_AQI},
  };
  memcpy(snapshot->sensors, sensors, sizeof(sensors));

  /* Weather placeholder defaults so the page is never blank before first
   * fetch. Mirrors the sensor-page set_sensor_missing pattern. */
  strlcpy(snapshot->date, "--", sizeof(snapshot->date));
  strlcpy(snapshot->weekday, "--", sizeof(snapshot->weekday));
  strlcpy(snapshot->time, "--", sizeof(snapshot->time));
  strlcpy(snapshot->location, "--", sizeof(snapshot->location));
  strlcpy(snapshot->weather_text, "--", sizeof(snapshot->weather_text));
  strlcpy(snapshot->temperature, "--", sizeof(snapshot->temperature));
  strlcpy(snapshot->humidity, "--", sizeof(snapshot->humidity));
  strlcpy(snapshot->weather_temperature, "--", sizeof(snapshot->weather_temperature));
  strlcpy(snapshot->weather_humidity, "--", sizeof(snapshot->weather_humidity));
  strlcpy(snapshot->wind, "--", sizeof(snapshot->wind));
  strlcpy(snapshot->high, "--", sizeof(snapshot->high));
  strlcpy(snapshot->low, "--", sizeof(snapshot->low));
  strlcpy(snapshot->pressure, "--", sizeof(snapshot->pressure));
  strlcpy(snapshot->feels_like, "--", sizeof(snapshot->feels_like));
  strlcpy(snapshot->precip, "--", sizeof(snapshot->precip));
  for (int i = 0; i < DESKMON_DISPLAY_HOURLY_COUNT; ++i) {
    strlcpy(snapshot->hourly[i].time, "--", sizeof(snapshot->hourly[i].time));
    strlcpy(snapshot->hourly[i].weather, "--", sizeof(snapshot->hourly[i].weather));
  }
  for (int i = 0; i < DESKMON_DISPLAY_DAILY_COUNT; ++i) {
    strlcpy(snapshot->daily[i].date, "--", sizeof(snapshot->daily[i].date));
    strlcpy(snapshot->daily[i].low, "--", sizeof(snapshot->daily[i].low));
    strlcpy(snapshot->daily[i].high, "--", sizeof(snapshot->daily[i].high));
    snapshot->daily[i].icon = DESKMON_WEATHER_ICON_SUN;
  }
  snapshot->weather_valid = false;
}

lv_obj_t *deskmon_page_create(deskmon_page_id_t page, lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  deskmon_page_id_t safe_page = (page >= 0 && page < DESKMON_PAGE_COUNT) ? page : DESKMON_PAGE_SUMMARY;
  lv_obj_t *container = lv_obj_create(parent);
  style_plain(container);
  lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

  switch (safe_page) {
  case DESKMON_PAGE_WEATHER:
    render_weather(container, snapshot);
    break;
  case DESKMON_PAGE_SENSOR:
    render_sensor(container, snapshot);
    break;
  case DESKMON_PAGE_MEMO:
    render_memo(container);
    break;
  case DESKMON_PAGE_ALBUM:
    render_album(container);
    break;
  case DESKMON_PAGE_SUMMARY:
  default:
    render_summary(container, snapshot);
    break;
  }

  return container;
}
