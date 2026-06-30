#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>

#include "app/app_config.h"

typedef struct cJSON cJSON;

cJSON *create_time_status_json(void);
esp_err_t deskmon_time_apply_config(const deskmon_config_t *config);
esp_err_t deskmon_time_set_epoch(int64_t epoch_sec);
char *deskmon_time_json(void);
