#include "ui/ui_pages.h"

static const char *const s_page_titles[DESKMON_PAGE_COUNT] = {
    "Summary / 汇总", "Weather / 天气", "Sensors / 传感器", "Memo / 备忘录", "Album / 相册",
};

static const char *const s_page_placeholders[DESKMON_PAGE_COUNT] = {
    "Date / 日期\nWeather brief / 简要天气\nMemo brief / 简要备忘\nSensor brief / 简要传感器数据",
    "Current weather / 当前天气\nTemperature / 温度\nHumidity / 湿度\nWind / 风\nAQI / 空气质量",
    "Current / 当前\nMax / 最大\nMin / 最小\nAverage / 平均\nCurve placeholder / 曲线占位",
    "Memo list / 备忘列表\nNext item / 下一条\nUpdated by web control / 由网页管理更新",
    "TF photo list / TF照片列表\nUploaded photos / 上传照片\nPreview placeholder / 预览占位",
};

const char *deskmon_page_title(deskmon_page_id_t page) {
  if (page < 0 || page >= DESKMON_PAGE_COUNT) {
    return "Unknown / 未知";
  }
  return s_page_titles[page];
}

lv_obj_t *deskmon_page_create(deskmon_page_id_t page, lv_obj_t *parent) {
  deskmon_page_id_t safe_page = (page >= 0 && page < DESKMON_PAGE_COUNT) ? page : DESKMON_PAGE_SUMMARY;
  lv_obj_t *container = lv_obj_create(parent);
  lv_obj_t *title = lv_label_create(container);
  lv_obj_t *body = lv_label_create(container);

  lv_label_set_text(title, deskmon_page_title(safe_page));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
  lv_label_set_text(body, s_page_placeholders[safe_page]);
  lv_obj_align(body, LV_ALIGN_TOP_LEFT, 12, 48);
  return container;
}
