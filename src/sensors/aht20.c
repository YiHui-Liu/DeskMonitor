#include "sensors/aht20.h"

#include <stdint.h>

#include "bsp/bsp_io.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors/sensor_i2c.h"

#define AHT20_TIMEOUT_MS 100
#define AHT20_STATUS_BUSY 0x80
#define AHT20_STATUS_CALIBRATED 0x08

static esp_err_t aht20_read_status(i2c_master_dev_handle_t device, uint8_t *status) {
  return i2c_master_receive(device, status, 1, AHT20_TIMEOUT_MS);
}

static esp_err_t aht20_wait_ready(i2c_master_dev_handle_t device) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    uint8_t status = 0;
    esp_err_t err = aht20_read_status(device, &status);
    if (err != ESP_OK) {
      return err;
    }
    if ((status & AHT20_STATUS_BUSY) == 0) {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t deskmon_aht20_read(float *temperature_c, float *humidity_rh) {
  if (temperature_c == NULL || humidity_rh == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  i2c_master_dev_handle_t device = NULL;
  esp_err_t err = deskmon_sensor_i2c_add_device(DESKMON_AHT20_I2C_ADDR, &device);
  if (err != ESP_OK) {
    return err;
  }

  uint8_t status = 0;
  err = aht20_read_status(device, &status);
  if (err == ESP_OK && (status & AHT20_STATUS_CALIBRATED) == 0) {
    const uint8_t init_command[] = {0xBE, 0x08, 0x00};
    err = i2c_master_transmit(device, init_command, sizeof(init_command), AHT20_TIMEOUT_MS);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (err == ESP_OK) {
    const uint8_t trigger_command[] = {0xAC, 0x33, 0x00};
    err = i2c_master_transmit(device, trigger_command, sizeof(trigger_command), AHT20_TIMEOUT_MS);
  }
  if (err == ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(80));
    err = aht20_wait_ready(device);
  }

  uint8_t data[6] = {0};
  if (err == ESP_OK) {
    err = i2c_master_receive(device, data, sizeof(data), AHT20_TIMEOUT_MS);
  }

  deskmon_sensor_i2c_remove_device(device);
  if (err != ESP_OK) {
    return err;
  }
  if ((data[0] & AHT20_STATUS_BUSY) != 0) {
    return ESP_ERR_TIMEOUT;
  }

  uint32_t humidity_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
  uint32_t temperature_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
  *humidity_rh = ((float)humidity_raw * 100.0f) / 1048576.0f;
  *temperature_c = ((float)temperature_raw * 200.0f) / 1048576.0f - 50.0f;
  return ESP_OK;
}
