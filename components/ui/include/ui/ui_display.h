#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/ui_carousel.h"

#include "esp_err.h"

typedef struct {
  bool enabled_pages[DESKMON_PAGE_COUNT];
  uint32_t carousel_interval_sec;
} deskmon_display_config_t;

esp_err_t deskmon_display_init(const deskmon_display_config_t *config);
bool deskmon_display_is_enabled(void);
