#pragma once

#include "esp_err.h"

esp_err_t deskmon_aht20_read(float *temperature_c, float *humidity_rh);
