#include "app/app_diagnostics.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cJSON.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_littlefs.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_system.h>

#include "app/app_storage.h"
#include "app/app_wifi.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_io.h"
#include "sensors/aht20.h"
#include "sensors/ens160.h"
#include "sensors/tsl2591.h"

static const char *TAG = "deskmon_diag";
static const int I2C_PROBE_TIMEOUT_MS = 50;

static bool i2c_device_found(unsigned address) {
  return deskmon_i2c_probe((uint16_t)address, I2C_PROBE_TIMEOUT_MS) == ESP_OK;
}

static void add_wifi_section(cJSON *root) {
  cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
  deskmon_wifi_status_t wifi_status = deskmon_wifi_status();
  cJSON_AddBoolToObject(wifi, "sta_connected", wifi_status.sta_connected);
  cJSON_AddBoolToObject(wifi, "ap_started", wifi_status.ap_started);
  cJSON_AddStringToObject(wifi, "sta_ip", wifi_status.sta_ip);
}

static cJSON *create_quantity_row(const char *name, const char *sensor, unsigned address, const char *status,
                                  const char *unit) {
  char address_text[8];
  snprintf(address_text, sizeof(address_text), "0x%02X", address);

  cJSON *row = cJSON_CreateObject();
  if (row == NULL) {
    return NULL;
  }

  cJSON_AddStringToObject(row, "name", name);
  cJSON_AddStringToObject(row, "sensor", sensor);
  cJSON_AddStringToObject(row, "address", address_text);
  cJSON_AddStringToObject(row, "status", status);
  cJSON_AddStringToObject(row, "unit", unit);
  return row;
}

static void add_quantity_number(cJSON *quantities, const char *name, const char *sensor, unsigned address,
                                const char *status, double reading, const char *unit) {
  cJSON *row = create_quantity_row(name, sensor, address, status, unit);
  if (row == NULL) {
    return;
  }
  cJSON_AddNumberToObject(row, "reading", reading);
  cJSON_AddItemToArray(quantities, row);
}

static void add_quantity_na(cJSON *quantities, const char *name, const char *sensor, unsigned address,
                            const char *status, const char *unit) {
  cJSON *row = create_quantity_row(name, sensor, address, status, unit);
  if (row == NULL) {
    return;
  }
  cJSON_AddStringToObject(row, "reading", "n/a");
  cJSON_AddItemToArray(quantities, row);
}

static void add_tsl2591_quantities(cJSON *quantities) {
  const unsigned address = DESKMON_TSL2591_I2C_ADDR;
  if (!i2c_device_found(address)) {
    add_quantity_na(quantities, "Illuminance", "TSL2591", address, "missing", "lux");
    return;
  }

  float lux = 0.0f;
  esp_err_t err = deskmon_tsl2591_read_lux(&lux);
  if (err != ESP_OK) {
    add_quantity_na(quantities, "Illuminance", "TSL2591", address, "read-error", "lux");
    return;
  }
  add_quantity_number(quantities, "Illuminance", "TSL2591", address, "ok", lux, "lux");
}

static void add_aht20_quantities(cJSON *quantities) {
  const unsigned address = DESKMON_AHT20_I2C_ADDR;
  if (!i2c_device_found(address)) {
    add_quantity_na(quantities, "Temperature", "AHT20", address, "missing", "°C");
    add_quantity_na(quantities, "Humidity", "AHT20", address, "missing", "%RH");
    return;
  }

  float temperature_c = 0.0f;
  float humidity_rh = 0.0f;
  esp_err_t err = deskmon_aht20_read(&temperature_c, &humidity_rh);
  if (err != ESP_OK) {
    add_quantity_na(quantities, "Temperature", "AHT20", address, "read-error", "°C");
    add_quantity_na(quantities, "Humidity", "AHT20", address, "read-error", "%RH");
    return;
  }
  add_quantity_number(quantities, "Temperature", "AHT20", address, "ok", temperature_c, "°C");
  add_quantity_number(quantities, "Humidity", "AHT20", address, "ok", humidity_rh, "%RH");
}

static void add_ens160_quantities(cJSON *quantities) {
  const unsigned address = DESKMON_ENS160_I2C_ADDR;
  if (!i2c_device_found(address)) {
    add_quantity_na(quantities, "AQI", "ENS160", address, "missing", "index");
    add_quantity_na(quantities, "TVOC", "ENS160", address, "missing", "ppb");
    add_quantity_na(quantities, "eCO2", "ENS160", address, "missing", "ppm");
    return;
  }

  deskmon_ens160_reading_t reading = {0};
  esp_err_t err = deskmon_ens160_read(&reading);
  if (err != ESP_OK) {
    add_quantity_na(quantities, "AQI", "ENS160", address, "read-error", "index");
    add_quantity_na(quantities, "TVOC", "ENS160", address, "read-error", "ppb");
    add_quantity_na(quantities, "eCO2", "ENS160", address, "read-error", "ppm");
    return;
  }
  add_quantity_number(quantities, "AQI", "ENS160", address, "ok", reading.aqi, "index");
  add_quantity_number(quantities, "TVOC", "ENS160", address, "ok", reading.tvoc_ppb, "ppb");
  add_quantity_number(quantities, "eCO2", "ENS160", address, "ok", reading.eco2_ppm, "ppm");
}

static void add_quantities_section(cJSON *root) {
  cJSON *quantities = cJSON_AddArrayToObject(root, "quantities");
  add_tsl2591_quantities(quantities);
  add_aht20_quantities(quantities);
  add_ens160_quantities(quantities);
}

static void add_io_section(cJSON *root) {
  cJSON *io = cJSON_AddObjectToObject(root, "io");
  cJSON *button1 = cJSON_AddObjectToObject(io, "button1");
  cJSON_AddNumberToObject(button1, "gpio", DESKMON_BUTTON_1_GPIO);
  cJSON_AddBoolToObject(button1, "enabled", DESKMON_BUTTONS_ENABLED != 0);

  cJSON *button2 = cJSON_AddObjectToObject(io, "button2");
  cJSON_AddNumberToObject(button2, "gpio", DESKMON_BUTTON_2_GPIO);
  cJSON_AddBoolToObject(button2, "enabled", DESKMON_BUTTONS_ENABLED != 0);

  cJSON *display = cJSON_AddObjectToObject(io, "display");
  cJSON_AddBoolToObject(display, "enabled", DESKMON_DISPLAY_ENABLED != 0);
  cJSON_AddNumberToObject(display, "width", DESKMON_DISPLAY_WIDTH);
  cJSON_AddNumberToObject(display, "height", DESKMON_DISPLAY_HEIGHT);
  cJSON_AddNumberToObject(display, "mosi", DESKMON_DISPLAY_MOSI_GPIO);
  cJSON_AddNumberToObject(display, "sclk", DESKMON_DISPLAY_SCLK_GPIO);
  cJSON_AddNumberToObject(display, "cs", DESKMON_DISPLAY_CS_GPIO);
  cJSON_AddNumberToObject(display, "dc", DESKMON_DISPLAY_DC_GPIO);
  cJSON_AddNumberToObject(display, "rst", DESKMON_DISPLAY_RST_GPIO);
  cJSON_AddNumberToObject(display, "backlight", DESKMON_DISPLAY_BL_GPIO);
}

static void add_memory_section(cJSON *system) {
  cJSON *memory = cJSON_AddObjectToObject(system, "memory");
  cJSON_AddNumberToObject(memory, "free_heap_bytes", esp_get_free_heap_size());
  cJSON_AddNumberToObject(memory, "minimum_free_heap_bytes", esp_get_minimum_free_heap_size());
  cJSON_AddNumberToObject(memory, "largest_free_block_bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void add_storage_section(cJSON *system) {
  cJSON *storage = cJSON_AddObjectToObject(system, "storage");
  size_t total_bytes = 0;
  size_t used_bytes = 0;
  esp_err_t err = esp_littlefs_info("littlefs", &total_bytes, &used_bytes);
  cJSON_AddStringToObject(storage, "label", "littlefs");
  cJSON_AddStringToObject(storage, "mount", DESKMON_STORAGE_BASE_PATH);
  cJSON_AddStringToObject(storage, "status", err == ESP_OK ? "ok" : esp_err_to_name(err));
  cJSON_AddNumberToObject(storage, "total_bytes", total_bytes);
  cJSON_AddNumberToObject(storage, "used_bytes", used_bytes);
  cJSON_AddNumberToObject(storage, "free_bytes", total_bytes > used_bytes ? total_bytes - used_bytes : 0);
}

static void add_partitions_section(cJSON *system) {
  cJSON *partitions = cJSON_AddArrayToObject(system, "partitions");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it != NULL) {
    const esp_partition_t *partition = esp_partition_get(it);
    cJSON *row = cJSON_CreateObject();
    if (row != NULL) {
      cJSON_AddStringToObject(row, "label", partition->label);
      cJSON_AddNumberToObject(row, "type", partition->type);
      cJSON_AddNumberToObject(row, "subtype", partition->subtype);
      cJSON_AddNumberToObject(row, "address", partition->address);
      cJSON_AddNumberToObject(row, "size_bytes", partition->size);
      cJSON_AddItemToArray(partitions, row);
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

static void add_system_section(cJSON *root) {
  cJSON *system = cJSON_AddObjectToObject(root, "system");
  add_memory_section(system);
  add_storage_section(system);
  add_partitions_section(system);
}

char *deskmon_diagnostics_json(void) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return NULL;
  }

  add_wifi_section(root);
  add_quantities_section(root);
  add_io_section(root);
  add_system_section(root);

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json;
}

void deskmon_diagnostics_log(void) {
  ESP_LOGI(TAG, "I2C: SDA=%d SCL=%d freq=%d", DESKMON_I2C_SDA_GPIO, DESKMON_I2C_SCL_GPIO, DESKMON_I2C_FREQ_HZ);

  size_t count = 0;
  const deskmon_i2c_device_info_t *known = deskmon_i2c_known_devices(&count);
  for (size_t i = 0; i < count; ++i) {
    ESP_LOGI(TAG, "I2C probe %s at 0x%02X: %s", known[i].name, known[i].address,
             i2c_device_found(known[i].address) ? "found" : "missing");
  }

  ESP_LOGI(TAG, "Button1 GPIO=%d enabled=%d", DESKMON_BUTTON_1_GPIO, DESKMON_BUTTONS_ENABLED);
  ESP_LOGI(TAG, "Button2 GPIO=%d enabled=%d", DESKMON_BUTTON_2_GPIO, DESKMON_BUTTONS_ENABLED);
  ESP_LOGI(TAG, "Display enabled=%d size=%dx%d", DESKMON_DISPLAY_ENABLED, DESKMON_DISPLAY_WIDTH,
           DESKMON_DISPLAY_HEIGHT);
}
