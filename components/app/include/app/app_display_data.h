#pragma once

#include "app/app_config.h"
#include "ui/ui_pages.h"

#include "esp_err.h"

esp_err_t deskmon_display_data_start(const deskmon_config_t *config);
bool deskmon_display_data_snapshot(deskmon_display_snapshot_t *snapshot, void *ctx);
