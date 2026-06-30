#include "sensors/ens160.h"

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/bsp_io.h"
#include "sensors/aht20.h"
#include "sensors/sensor_i2c.h"

#define ENS160_TIMEOUT_MS 100

#define ENS160_REG_PART_ID 0x00
#define ENS160_REG_OPMODE 0x10
#define ENS160_REG_REF_TEMP 0x13
#define ENS160_REG_REF_RH 0x15
#define ENS160_REG_DATA_STATUS 0x20
#define ENS160_REG_DATA_AQI 0x21
#define ENS160_REG_GPR_READ 0x48

#define ENS160_OPMODE_STANDARD 0x02
#define ENS160_PART_ID 0x0160

#define ENS160_STATUS_NEW_GPR_MASK 0x01
#define ENS160_STATUS_NEW_DATA_MASK 0x02
#define ENS160_STATUS_VALIDITY_MASK 0x0C
#define ENS160_STATUS_VALIDITY_NORMAL 0x00
#define ENS160_STATUS_STATER_MASK 0x40

static esp_err_t ens160_write_register(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value) {
  const uint8_t data[] = {reg, value};
  return i2c_master_transmit(device, data, sizeof(data), ENS160_TIMEOUT_MS);
}

static esp_err_t ens160_read_registers(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *data, size_t length) {
  return i2c_master_transmit_receive(device, &reg, 1, data, length, ENS160_TIMEOUT_MS);
}

static esp_err_t ens160_wait_data_ready(i2c_master_dev_handle_t device) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    uint8_t status = 0;
    esp_err_t err = ens160_read_registers(device, ENS160_REG_DATA_STATUS, &status, 1);
    if (err != ESP_OK) {
      return err;
    }
    if ((status & ENS160_STATUS_STATER_MASK) != 0 ||
        (status & ENS160_STATUS_VALIDITY_MASK) != ENS160_STATUS_VALIDITY_NORMAL) {
      return ESP_ERR_INVALID_RESPONSE;
    }
    if ((status & ENS160_STATUS_NEW_DATA_MASK) != 0) {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t deskmon_ens160_read(deskmon_ens160_reading_t *reading) {
  if (reading == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Initialize the I2C device for the ENS160 sensor
  i2c_master_dev_handle_t device = NULL;
  esp_err_t err = deskmon_sensor_i2c_add_device(DESKMON_ENS160_I2C_ADDR, &device);
  if (err != ESP_OK) {
    return err;
  }

  // Read the part ID
  uint8_t part_id_data[2] = {0};
  err = ens160_read_registers(device, ENS160_REG_PART_ID, part_id_data, sizeof(part_id_data));
  uint16_t part_id = (uint16_t)part_id_data[0] | ((uint16_t)part_id_data[1] << 8);
  if (err == ESP_OK && part_id != ENS160_PART_ID) {
    err = ESP_ERR_INVALID_RESPONSE;
  }

  // Set Operating Mode, 0x00 Deep Sleep, 0x01 Idle, 0x02 Standard, 0xF0 Reset
  if (err == ESP_OK) {
    err = ens160_write_register(device, ENS160_REG_OPMODE, ENS160_OPMODE_STANDARD);
  }
  if (err == ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  // Set Reference Temperature and Relative Humidity
  float temperature_c = 0.0f;
  float humidity_rh = 0.0f;
  err = deskmon_aht20_read(&temperature_c, &humidity_rh);
  if (err == ESP_OK) {
    // Low byte first, then high byte
    uint16_t temp_value = (uint16_t)((temperature_c + 273.15f) * 64.0f); // Convert to Kelvin and scale
    uint16_t rh_value = (uint16_t)(humidity_rh * 512.0f);                // Scale to 0-51200 range
    err = ens160_write_register(device, ENS160_REG_REF_TEMP, (uint8_t)(temp_value & 0xFF));
    err = ens160_write_register(device, ENS160_REG_REF_TEMP + 1, (uint8_t)((temp_value >> 8) & 0xFF));
    err = ens160_write_register(device, ENS160_REG_REF_RH, (uint8_t)(rh_value & 0xFF));
    err = ens160_write_register(device, ENS160_REG_REF_RH + 1, (uint8_t)((rh_value >> 8) & 0xFF));
  } else
    return err;

  if (err == ESP_OK) {
    err = ens160_wait_data_ready(device);
  }

  // Read the AQI (1 byte), TVOC (2 bytes), and eCO2 (2 bytes) data
  uint8_t data[5] = {0};
  if (err == ESP_OK) {
    err = ens160_read_registers(device, ENS160_REG_DATA_AQI, data, sizeof(data));
  }

  deskmon_sensor_i2c_remove_device(device);
  if (err != ESP_OK) {
    return err;
  }
  reading->aqi = data[0];
  reading->tvoc_ppb = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
  reading->eco2_ppm = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
  return ESP_OK;
}
