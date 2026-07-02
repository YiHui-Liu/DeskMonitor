#pragma once

#include <stdbool.h>

#include <esp_err.h>

#include "app/app_config.h"

typedef struct {
  bool sta_connected;
  bool ap_started;
  char sta_ip[16];
} deskmon_wifi_status_t;

esp_err_t deskmon_wifi_start(const deskmon_config_t *config);
esp_err_t deskmon_wifi_apply_sta(const deskmon_config_t *config);
deskmon_wifi_status_t deskmon_wifi_status(void);
