#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>

#include "ui/ui_carousel.h"
#include "ui/ui_pages.h"

typedef bool (*deskmon_display_snapshot_provider_t)(deskmon_display_snapshot_t *snapshot, void *ctx);

typedef struct {
  bool enabled_pages[DESKMON_PAGE_COUNT];
  uint32_t carousel_interval_sec;
  deskmon_display_snapshot_provider_t snapshot_provider;
  void *snapshot_ctx;
} deskmon_display_config_t;

esp_err_t deskmon_display_init(const deskmon_display_config_t *config);
void deskmon_display_apply_config(const bool enabled_pages[DESKMON_PAGE_COUNT], uint32_t carousel_interval_sec);
bool deskmon_display_is_enabled(void);
