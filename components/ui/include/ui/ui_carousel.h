#pragma once

#include <stdbool.h>

typedef enum {
  DESKMON_PAGE_SUMMARY = 0,
  DESKMON_PAGE_WEATHER,
  DESKMON_PAGE_SENSOR,
  DESKMON_PAGE_MEMO,
  DESKMON_PAGE_ALBUM,
  DESKMON_PAGE_COUNT,
} deskmon_page_id_t;

const char *deskmon_page_title(deskmon_page_id_t page);
deskmon_page_id_t deskmon_carousel_next_enabled(deskmon_page_id_t current, const bool enabled[DESKMON_PAGE_COUNT]);
