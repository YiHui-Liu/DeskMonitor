#include "ui/ui_carousel.h"

static const char *const s_page_titles[DESKMON_PAGE_COUNT] = {
    "汇总",
    "天气",
    "传感器",
    "备忘",
    "相册",
};

const char *deskmon_page_title(deskmon_page_id_t page) {
  if (page < 0 || page >= DESKMON_PAGE_COUNT) {
    return "汇总";
  }
  return s_page_titles[page];
}

deskmon_page_id_t deskmon_carousel_next_enabled(deskmon_page_id_t current, const bool enabled[DESKMON_PAGE_COUNT]) {
  if (enabled == 0) {
    return DESKMON_PAGE_SUMMARY;
  }

  deskmon_page_id_t first_enabled = DESKMON_PAGE_SUMMARY;
  bool found_enabled = false;
  for (int i = 0; i < DESKMON_PAGE_COUNT; ++i) {
    if (enabled[i]) {
      first_enabled = (deskmon_page_id_t)i;
      found_enabled = true;
      break;
    }
  }
  if (!found_enabled) {
    return DESKMON_PAGE_SUMMARY;
  }

  const int start = (current >= 0 && current < DESKMON_PAGE_COUNT) ? current : first_enabled;
  for (int step = 1; step <= DESKMON_PAGE_COUNT; ++step) {
    const int candidate = (start + step) % DESKMON_PAGE_COUNT;
    if (enabled[candidate]) {
      return (deskmon_page_id_t)candidate;
    }
  }
  return first_enabled;
}
