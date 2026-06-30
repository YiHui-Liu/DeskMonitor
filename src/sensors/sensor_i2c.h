#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t deskmon_sensor_i2c_add_device(uint16_t address, i2c_master_dev_handle_t *device);
void deskmon_sensor_i2c_remove_device(i2c_master_dev_handle_t device);
