#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
  const char *name;
  unsigned address;
} deskmon_i2c_device_info_t;

esp_err_t deskmon_i2c_init(void);
i2c_master_bus_handle_t deskmon_i2c_bus(void);
esp_err_t deskmon_i2c_probe(uint16_t address, int timeout_ms);
const deskmon_i2c_device_info_t *deskmon_i2c_known_devices(size_t *count);
