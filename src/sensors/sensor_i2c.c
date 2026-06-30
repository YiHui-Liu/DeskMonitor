#include "sensors/sensor_i2c.h"

#include <stddef.h>

#include "bsp/bsp_i2c.h"
#include "bsp/bsp_io.h"

esp_err_t deskmon_sensor_i2c_add_device(uint16_t address, i2c_master_dev_handle_t *device) {
  i2c_master_bus_handle_t bus = deskmon_i2c_bus();
  if (bus == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = DESKMON_I2C_FREQ_HZ,
  };
  return i2c_master_bus_add_device(bus, &config, device);
}

void deskmon_sensor_i2c_remove_device(i2c_master_dev_handle_t device) {
  if (device != NULL) {
    (void)i2c_master_bus_rm_device(device);
  }
}
