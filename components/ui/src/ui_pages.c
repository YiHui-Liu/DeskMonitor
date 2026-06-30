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
  return label;
}

static void label_center(lv_obj_t *parent, const char *text, int32_t y, lv_color_t color) {
  lv_obj_t *label = label_create(parent, text, 0, y, color);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static void metric_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, const char *icon, const char *name,
                        const char *reading, const char *status, lv_color_t accent) {
  lv_obj_t *card = card_create(parent, x, y, w, 70);
  label_create(card, icon, 4, 2, accent);
  label_create(card, name, 28, 4, lv_color_hex(0x1F2937));
  label_center(card, reading, 26, lv_color_hex(0x111827));
  label_center(card, status, 50, accent);
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
  metric_card(parent, 8, 164, 72, "温", "温度", "27.4°C", "舒适", lv_color_hex(0xEF4444));
  metric_card(parent, 86, 164, 72, "湿", "湿度", "62%", "舒适", lv_color_hex(0x228BE6));
  metric_card(parent, 164, 164, 72, "光", "光照", "512lx", "适中", lv_color_hex(0xF59E0B));
  metric_card(parent, 242, 164, 72, "C", "CO2", "724", "良好", lv_color_hex(0x22A652));
  metric_card(parent, 320, 164, 72, "气", "TVOC", "0.38", "优", lv_color_hex(0x7C5CC4));
  metric_card(parent, 398, 164, 74, "空", "AQI", "32", "优", lv_color_hex(0x2D9CA3));
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
  metric_card(parent, 8, 8, 70, "温", "温度", "27.4", "舒适", lv_color_hex(0xEF4444));
  metric_card(parent, 86, 8, 70, "湿", "湿度", "62%", "舒适", lv_color_hex(0x228BE6));
  metric_card(parent, 164, 8, 70, "光", "光照", "512", "适中", lv_color_hex(0xF59E0B));
  metric_card(parent, 242, 8, 70, "C", "CO2", "724", "良好", lv_color_hex(0x22A652));
  metric_card(parent, 320, 8, 70, "气", "TVOC", "0.38", "优", lv_color_hex(0x7C5CC4));
  metric_card(parent, 398, 8, 74, "空", "AQI", "32", "优", lv_color_hex(0x2D9CA3));

  label_create(parent, "24小时趋势", 14, 88, lv_color_hex(0x155CC9));
  const char *charts[] = {"温度  24 26 29 30 28 27", "湿度  78 70 56 45 58 62", "光照  0 80 860 1800 512 0",
                          "CO2  520 610 724 910 680", "TVOC 0.18 0.28 0.56 0.38", "AQI  21 32 42 32"};
  for (int i = 0; i < 6; ++i) {
    lv_obj_t *card = card_create(parent, 8 + (i % 3) * 154, 114 + (i / 3) * 62, 146, 54);
    label_create(card, charts[i], 6, 18, lv_color_hex(0x1F2937));
  }
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
