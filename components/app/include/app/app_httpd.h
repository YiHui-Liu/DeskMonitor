#pragma once

#include <esp_err.h>

#include "app/app_config.h"

esp_err_t deskmon_httpd_start(deskmon_config_t *config);
