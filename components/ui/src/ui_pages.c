#include "ui/ui_pages.h"
#include "ui/ui_font.h"

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

/* ---- Sensor page sparkline data (8 points each, x: 0-140 step 20, y: 0-40 inverted) ---- */

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
static void weather_day_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, const char *date, int cond,
                             const char *temp, lv_color_t accent) {
  lv_obj_t *card = card_create(parent, x, y, w, h);
  label_center(card, date, 5, accent);
  const int32_t ix = w / 2 - 8;
  const int32_t iy = h / 2 - 8;
  if (cond == 0) {
    draw_weather_icon(card, ix, iy, lv_color_hex(0xF5B800));
  } else {
    icon_fill(card, ix, iy + 4, 14, 8, lv_color_hex(0x9CB3D6), 4);
    icon_fill(card, ix + 4, iy, 12, 9, lv_color_hex(0xB7C8E0), 4);
    if (cond == 2) {
      icon_fill(card, ix + 4, iy + 12, 1, 4, lv_color_hex(0x228BE6), 0);
      icon_fill(card, ix + 9, iy + 12, 1, 4, lv_color_hex(0x228BE6), 0);
    }
  }
  label_center(card, temp, h - 16, lv_color_hex(0x111827));
}

void deskmon_display_snapshot_defaults(deskmon_display_snapshot_t *snapshot) {
  if (snapshot == NULL) {
    return;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  strlcpy(snapshot->date, "2026-06-30", sizeof(snapshot->date));
  strlcpy(snapshot->weekday, "星期二", sizeof(snapshot->weekday));
  strlcpy(snapshot->time, "10:30", sizeof(snapshot->time));
  strlcpy(snapshot->location, "深圳市 南山区", sizeof(snapshot->location));
  strlcpy(snapshot->weather_text, "多云", sizeof(snapshot->weather_text));
  strlcpy(snapshot->temperature, "28°C", sizeof(snapshot->temperature));
  strlcpy(snapshot->humidity, "62%", sizeof(snapshot->humidity));
  strlcpy(snapshot->wind, "东南风 2.6m/s", sizeof(snapshot->wind));
  strlcpy(snapshot->high, "32°C", sizeof(snapshot->high));
  strlcpy(snapshot->low, "24°C", sizeof(snapshot->low));
  strlcpy(snapshot->pressure, "1008 hPa", sizeof(snapshot->pressure));
  strlcpy(snapshot->feels_like, "31°C", sizeof(snapshot->feels_like));
  strlcpy(snapshot->precip, "20%", sizeof(snapshot->precip));

  const char *times[DESKMON_DISPLAY_HOURLY_COUNT] = {"10:00", "11:00", "12:00", "13:00", "14:00", "15:00"};
  const char *hourly[DESKMON_DISPLAY_HOURLY_COUNT] = {"晴 30°",   "晴 31°",   "多云 31°",
                                                      "多云 32°", "小雨 30°", "多云 29°"};
  for (int i = 0; i < DESKMON_DISPLAY_HOURLY_COUNT; ++i) {
    strlcpy(snapshot->hourly[i].time, times[i], sizeof(snapshot->hourly[i].time));
    strlcpy(snapshot->hourly[i].weather, hourly[i], sizeof(snapshot->hourly[i].weather));
  }

  const deskmon_display_daily_t daily[DESKMON_DISPLAY_DAILY_COUNT] = {
      {.date = "6月30", .temp = "24~33°", .icon = DESKMON_WEATHER_ICON_SUN},
      {.date = "7月1", .temp = "25~34°", .icon = DESKMON_WEATHER_ICON_CLOUD},
      {.date = "7月2", .temp = "24~30°", .icon = DESKMON_WEATHER_ICON_RAIN},
      {.date = "7月3", .temp = "25~32°", .icon = DESKMON_WEATHER_ICON_CLOUD},
      {.date = "7月4", .temp = "25~33°", .icon = DESKMON_WEATHER_ICON_SUN},
  };
  memcpy(snapshot->daily, daily, sizeof(daily));

  const deskmon_display_sensor_t sensors[DESKMON_DISPLAY_SENSOR_COUNT] = {
      {.name = "温度",
       .icon = DESKMON_SENSOR_ICON_TEMPERATURE,
       .reading = "27.4°C",
       .status = "舒适",
       .stats = "最大 29 | 最小 24 | 平均 27",
       .valid = true},
      {.name = "湿度",
       .icon = DESKMON_SENSOR_ICON_HUMIDITY,
       .reading = "62%",
       .status = "舒适",
       .stats = "最大 70 | 最小 50 | 平均 60",
       .valid = true},
      {.name = "光照",
       .icon = DESKMON_SENSOR_ICON_LIGHT,
       .reading = "512lx",
       .status = "适中",
       .stats = "最大 1800 | 最小 0 | 平均 760",
       .valid = true},
      {.name = "CO2",
       .icon = DESKMON_SENSOR_ICON_CO2,
       .reading = "724ppm",
       .status = "良好",
       .stats = "最大 900 | 最小 520 | 平均 680",
       .valid = true},
      {.name = "TVOC",
       .icon = DESKMON_SENSOR_ICON_TVOC,
       .reading = "0.38",
       .status = "优",
       .stats = "最大 0.50 | 最小 0.15 | 平均 0.33",
       .valid = true},
      {.name = "AQI",
       .icon = DESKMON_SENSOR_ICON_AQI,
       .reading = "32",
       .status = "优",
       .stats = "最大 42 | 最小 21 | 平均 31",
       .valid = true},
  };
  memcpy(snapshot->sensors, sensors, sizeof(sensors));
  snapshot->weather_valid = true;
  snapshot->sensors_valid = true;
}

static const deskmon_display_snapshot_t *snapshot_or_default(const deskmon_display_snapshot_t *snapshot,
                                                             deskmon_display_snapshot_t *fallback) {
  if (snapshot != NULL) {
    return snapshot;
  }
  deskmon_display_snapshot_defaults(fallback);
  return fallback;
}

static const lv_point_precise_t spark_temp[] = {{0, 30}, {20, 25},  {40, 18},  {60, 8},
                                                {80, 5}, {100, 12}, {120, 22}, {140, 28}};
static const lv_point_precise_t spark_humid[] = {{0, 10},  {20, 15},  {40, 22},  {60, 30},
                                                 {80, 35}, {100, 28}, {120, 18}, {140, 12}};
static const lv_point_precise_t spark_light[] = {{0, 38}, {20, 35},  {40, 25},  {60, 12},
                                                 {80, 5}, {100, 15}, {120, 30}, {140, 38}};
static const lv_point_precise_t spark_co2[] = {{0, 25}, {20, 20},  {40, 15},  {60, 8},
                                               {80, 5}, {100, 10}, {120, 18}, {140, 22}};
static const lv_point_precise_t spark_tvoc[] = {{0, 30},  {20, 28},  {40, 20},  {60, 5},
                                                {80, 15}, {100, 25}, {120, 22}, {140, 28}};
static const lv_point_precise_t spark_aqi[] = {{0, 20},  {20, 18},  {40, 22},  {60, 15},
                                               {80, 25}, {100, 20}, {120, 18}, {140, 22}};

static void sensor_pad(lv_obj_t *parent, int32_t x, int32_t y, const deskmon_display_sensor_t *sensor,
                       lv_color_t accent, const lv_point_precise_t *pts, uint16_t cnt) {
  lv_obj_t *card = card_create(parent, x, y, 153, 113);
  sensor_icon_drawer(sensor->icon)(card, 0, 0, accent);
  label_create(card, sensor->name, 22, 1, lv_color_hex(0x1F2937));
  label_create(card, sensor->reading, 78, 1, lv_color_hex(0x111827));
  label_create(card, sensor->status, 124, 1, sensor->valid ? accent : lv_color_hex(0x9CA3AF));
  label_create(card, sensor->stats, 0, 28, lv_color_hex(0x4B5563));

  draw_chart_axes(card, 0, 62, lv_color_hex(0xC4D2E3));
  lv_obj_t *line = lv_line_create(card);
  lv_line_set_points(line, pts, cnt);
  lv_obj_set_pos(line, 0, 62);
  lv_obj_set_style_line_color(line, accent, 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

static void render_summary(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  lv_obj_t *date = card_create(parent, 8, 6, 172, 72);
  draw_calendar_icon(date, 12, 18, lv_color_hex(0x256BBD));
  char date_text[40];
  snprintf(date_text, sizeof(date_text), "%s\n%s\n%s", snapshot->date, snapshot->weekday, snapshot->time);
  label_create(date, date_text, 40, 8, lv_color_hex(0x111827));

  lv_obj_t *weather = card_create(parent, 188, 6, 284, 72);
  draw_weather_icon(weather, 14, 14, lv_color_hex(0xF5B800));
  char weather_text[32];
  snprintf(weather_text, sizeof(weather_text), "%s\n%s", snapshot->temperature, snapshot->weather_text);
  label_create(weather, weather_text, 40, 10, lv_color_hex(0x111827));
  draw_humidity_icon(weather, 150, 8, lv_color_hex(0x228BE6));
  char humidity_text[24];
  snprintf(humidity_text, sizeof(humidity_text), "湿度 %s", snapshot->humidity);
  label_create(weather, humidity_text, 170, 10, lv_color_hex(0x1F2937));
  draw_wind_icon(weather, 150, 40, lv_color_hex(0x228BE6));
  label_create(weather, snapshot->wind, 170, 42, lv_color_hex(0x1F2937));

  lv_obj_t *memo = card_create(parent, 8, 86, 464, 46);
  label_create(memo, "备忘录摘要", 16, 10, lv_color_hex(0x155CC9));
  label_create(memo, "· 15:00 设备巡检    · 17:30 更换滤网    · 20:00 关闭新风", 112, 12, lv_color_hex(0x111827));

  sensor_summary_card(parent, 8, 142, 110, 88, &snapshot->sensors[0], lv_color_hex(0xEF4444));
  sensor_summary_card(parent, 126, 142, 110, 88, &snapshot->sensors[1], lv_color_hex(0x228BE6));
  sensor_summary_card(parent, 244, 142, 110, 88, &snapshot->sensors[2], lv_color_hex(0xF59E0B));
  sensor_summary_card(parent, 362, 142, 110, 88, &snapshot->sensors[3], lv_color_hex(0x22A652));
}

static void hourly_weather_row(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  lv_obj_t *hours = card_create(parent, 8, 106, 464, 58);
  for (int i = 0; i < DESKMON_DISPLAY_HOURLY_COUNT; ++i) {
    label_create(hours, snapshot->hourly[i].time, 10 + i * 74, 8, lv_color_hex(0x155CC9));
    label_create(hours, snapshot->hourly[i].weather, 10 + i * 74, 32, lv_color_hex(0x111827));
  }
}

static void render_weather(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  lv_obj_t *main = card_create(parent, 8, 6, 220, 92);
  label_create(main, snapshot->location, 12, 6, lv_color_hex(0x155CC9));
  draw_weather_icon(main, 18, 34, lv_color_hex(0xF5B800));
  label_create(main, snapshot->temperature, 54, 28, lv_color_hex(0x111827));
  label_create(main, snapshot->weather_text, 112, 40, lv_color_hex(0x111827));

  lv_obj_t *detail = card_create(parent, 236, 6, 236, 92);
  char row[48];
  snprintf(row, sizeof(row), "最高 %s   %s", snapshot->high, snapshot->wind);
  label_create(detail, row, 10, 8, lv_color_hex(0x111827));
  snprintf(row, sizeof(row), "最低 %s   气压 %s", snapshot->low, snapshot->pressure);
  label_create(detail, row, 10, 36, lv_color_hex(0x111827));
  snprintf(row, sizeof(row), "体感 %s   降水 %s", snapshot->feels_like, snapshot->precip);
  label_create(detail, row, 10, 64, lv_color_hex(0x111827));

  hourly_weather_row(parent, snapshot);

  for (int i = 0; i < DESKMON_DISPLAY_DAILY_COUNT; ++i) {
    weather_day_card(parent, 8 + i * 94, 172, 88, 62, snapshot->daily[i].date, snapshot->daily[i].icon,
                     snapshot->daily[i].temp, lv_color_hex(0x155CC9));
  }
}

static void render_sensor(lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  sensor_pad(parent, 4, 4, &snapshot->sensors[0], lv_color_hex(0xEF4444), spark_temp, 8);
  sensor_pad(parent, 163, 4, &snapshot->sensors[1], lv_color_hex(0x228BE6), spark_humid, 8);
  sensor_pad(parent, 322, 4, &snapshot->sensors[2], lv_color_hex(0xF59E0B), spark_light, 8);
  sensor_pad(parent, 4, 123, &snapshot->sensors[3], lv_color_hex(0x22A652), spark_co2, 8);
  sensor_pad(parent, 163, 123, &snapshot->sensors[4], lv_color_hex(0x7C5CC4), spark_tvoc, 8);
  sensor_pad(parent, 322, 123, &snapshot->sensors[5], lv_color_hex(0x2D9CA3), spark_aqi, 8);
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

lv_obj_t *deskmon_page_create(deskmon_page_id_t page, lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot) {
  deskmon_page_id_t safe_page = (page >= 0 && page < DESKMON_PAGE_COUNT) ? page : DESKMON_PAGE_SUMMARY;
  deskmon_display_snapshot_t fallback;
  const deskmon_display_snapshot_t *data = snapshot_or_default(snapshot, &fallback);
  lv_obj_t *container = lv_obj_create(parent);
  style_plain(container);
  lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

  switch (safe_page) {
  case DESKMON_PAGE_WEATHER:
    render_weather(container, data);
    break;
  case DESKMON_PAGE_SENSOR:
    render_sensor(container, data);
    break;
  case DESKMON_PAGE_MEMO:
    render_memo(container);
    break;
  case DESKMON_PAGE_ALBUM:
    render_album(container);
    break;
  case DESKMON_PAGE_SUMMARY:
  default:
    render_summary(container, data);
    break;
  }

  return container;
}
