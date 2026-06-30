#include "app/app_config.h"
#include "app/app_diagnostics.h"
#include "app/app_httpd.h"
#include "app/app_storage.h"
#include "app/app_wifi.h"
#include "bsp/bsp_i2c.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "desk_monitor";
static deskmon_config_t s_config;

static esp_err_t init_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
    err = nvs_flash_init();
  }
  return err;
}

void app_main(void) {
  ESP_ERROR_CHECK(init_nvs());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(deskmon_storage_mount());
  ESP_ERROR_CHECK(deskmon_config_load(&s_config));
  ESP_ERROR_CHECK(deskmon_i2c_init());
  deskmon_diagnostics_log();
  ESP_ERROR_CHECK(deskmon_wifi_start(&s_config));
  ESP_ERROR_CHECK(deskmon_httpd_start(&s_config));

  ESP_LOGI(TAG, "display, buttons, and TF album are reserved but not initialized");
}
