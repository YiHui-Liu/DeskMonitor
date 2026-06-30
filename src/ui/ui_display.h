#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t deskmon_display_init(void);
bool deskmon_display_is_enabled(void);
