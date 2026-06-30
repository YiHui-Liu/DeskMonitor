#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint8_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
} deskmon_ens160_reading_t;

esp_err_t deskmon_ens160_read(deskmon_ens160_reading_t *reading);
