#include "sensors/tsl2591.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_io.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors/sensor_i2c.h"

#define TSL2591_TIMEOUT_MS 100
#define TSL2591_COMMAND_NORMAL 0xA0
#define TSL2591_REG_ENABLE 0x00
#define TSL2591_REG_CONTROL 0x01
#define TSL2591_REG_ID 0x12
#define TSL2591_REG_CHAN0_LOW 0x14
#define TSL2591_ENABLE_POWER_ON 0x01
#define TSL2591_ENABLE_ALS_ON 0x02
#define TSL2591_ID_EXPECTED 0x50
#define TSL2591_INTEGRATION_MS 100.0f
#define TSL2591_GAIN_MULTIPLIER 1.0f

static esp_err_t tsl2591_write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value) {
  const uint8_t data[] = {TSL2591_COMMAND_NORMAL | reg, value};
  return i2c_master_transmit(device, data, sizeof(data), TSL2591_TIMEOUT_MS);
}

static esp_err_t tsl2591_read_registers(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *data, size_t length) {
  const uint8_t command = TSL2591_COMMAND_NORMAL | reg;
  return i2c_master_transmit_receive(device, &command, 1, data, length, TSL2591_TIMEOUT_MS);
}

esp_err_t deskmon_tsl2591_read_lux(float *lux) {
  if (lux == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  i2c_master_dev_handle_t device = NULL;
  esp_err_t err = deskmon_sensor_i2c_add_device(DESKMON_TSL2591_I2C_ADDR, &device);
  if (err != ESP_OK) {
    return err;
  }

  uint8_t id = 0;
  err = tsl2591_read_registers(device, TSL2591_REG_ID, &id, 1);
  if (err == ESP_OK && id != TSL2591_ID_EXPECTED) {
    err = ESP_ERR_INVALID_RESPONSE;
  }
  if (err == ESP_OK) {
    err = tsl2591_write_register(device, TSL2591_REG_ENABLE, TSL2591_ENABLE_POWER_ON);
  }
  if (err == ESP_OK) {
    err = tsl2591_write_register(device, TSL2591_REG_CONTROL, 0x00);
  }
  if (err == ESP_OK) {
    err = tsl2591_write_register(device, TSL2591_REG_ENABLE, TSL2591_ENABLE_POWER_ON | TSL2591_ENABLE_ALS_ON);
  }
  if (err == ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(120));
  }

  uint8_t data[4] = {0};
  if (err == ESP_OK) {
    err = tsl2591_read_registers(device, TSL2591_REG_CHAN0_LOW, data, sizeof(data));
  }
  (void)tsl2591_write_register(device, TSL2591_REG_ENABLE, 0x00);
  deskmon_sensor_i2c_remove_device(device);
  if (err != ESP_OK) {
    return err;
  }

  uint16_t channel0 = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  uint16_t channel1 = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
  float counts_per_lux = (TSL2591_INTEGRATION_MS * TSL2591_GAIN_MULTIPLIER) / 408.0f;
  float lux1 = ((float)channel0 - (1.64f * (float)channel1)) / counts_per_lux;
  float lux2 = ((0.59f * (float)channel0) - (0.86f * (float)channel1)) / counts_per_lux;
  float computed = lux1 > lux2 ? lux1 : lux2;
  *lux = computed > 0.0f ? computed : 0.0f;
  return ESP_OK;
}
