#pragma once

#include "lvgl.h"

typedef enum {
    DESKMON_PAGE_SUMMARY = 0,
    DESKMON_PAGE_WEATHER,
    DESKMON_PAGE_SENSOR,
    DESKMON_PAGE_MEMO,
    DESKMON_PAGE_ALBUM,
    DESKMON_PAGE_COUNT,
} deskmon_page_id_t;

lv_obj_t *deskmon_page_create(deskmon_page_id_t page, lv_obj_t *parent);
const char *deskmon_page_title(deskmon_page_id_t page);
