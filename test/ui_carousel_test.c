#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "ui/ui_carousel.h"

static void test_page_count_and_titles(void) {
  assert(DESKMON_PAGE_COUNT == 5);
  assert(strcmp(deskmon_page_title(DESKMON_PAGE_SUMMARY), "首页") == 0);
  assert(strcmp(deskmon_page_title(DESKMON_PAGE_WEATHER), "天气") == 0);
  assert(strcmp(deskmon_page_title(DESKMON_PAGE_SENSOR), "传感器") == 0);
  assert(strcmp(deskmon_page_title(DESKMON_PAGE_MEMO), "备忘") == 0);
  assert(strcmp(deskmon_page_title(DESKMON_PAGE_ALBUM), "相册") == 0);
}

static void test_next_enabled_page_wraps_all_pages(void) {
  const bool enabled[DESKMON_PAGE_COUNT] = {true, true, true, true, true};

  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_SUMMARY, enabled) == DESKMON_PAGE_WEATHER);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_WEATHER, enabled) == DESKMON_PAGE_SENSOR);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_SENSOR, enabled) == DESKMON_PAGE_MEMO);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_MEMO, enabled) == DESKMON_PAGE_ALBUM);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_ALBUM, enabled) == DESKMON_PAGE_SUMMARY);
}

static void test_next_enabled_page_skips_disabled_pages(void) {
  const bool enabled[DESKMON_PAGE_COUNT] = {true, false, true, false, true};

  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_SUMMARY, enabled) == DESKMON_PAGE_SENSOR);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_SENSOR, enabled) == DESKMON_PAGE_ALBUM);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_ALBUM, enabled) == DESKMON_PAGE_SUMMARY);
}

static void test_next_enabled_page_single_or_empty_is_stable(void) {
  const bool single[DESKMON_PAGE_COUNT] = {false, false, true, false, false};
  const bool empty[DESKMON_PAGE_COUNT] = {false, false, false, false, false};

  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_SENSOR, single) == DESKMON_PAGE_SENSOR);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_MEMO, single) == DESKMON_PAGE_SENSOR);
  assert(deskmon_carousel_next_enabled(DESKMON_PAGE_ALBUM, empty) == DESKMON_PAGE_SUMMARY);
}

int main(void) {
  test_page_count_and_titles();
  test_next_enabled_page_wraps_all_pages();
  test_next_enabled_page_skips_disabled_pages();
  test_next_enabled_page_single_or_empty_is_stable();
  return 0;
}
