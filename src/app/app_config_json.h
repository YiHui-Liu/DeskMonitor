#pragma once

#include "app/app_config.h"

char *deskmon_config_to_json(const deskmon_config_t *config);
deskmon_config_status_t deskmon_config_from_json(const char *json, deskmon_config_t *config);
