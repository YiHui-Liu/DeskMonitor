#include "app/app_ota.h"

#include <stdlib.h>
#include <string.h>

#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "deskmon_ota";

static void ota_task(void *arg) {
  char *url = arg;
  esp_http_client_config_t http_config = {
      .url = url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
      .timeout_ms = 15000,
  };
  esp_https_ota_config_t ota_config = {
      .http_config = &http_config,
      .partial_http_download = true,
      .max_http_request_size = 4096,
  };

  ESP_LOGI(TAG, "starting OTA from %s", url);
  esp_err_t err = esp_https_ota(&ota_config);
  free(url);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "OTA complete, restarting");
    esp_restart();
  }

  ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
  vTaskDelete(NULL);
}

esp_err_t deskmon_ota_start_url(const char *url) {
  if (url == NULL || (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0)) {
    return ESP_ERR_INVALID_ARG;
  }

  char *copy = strdup(url);
  if (copy == NULL) {
    return ESP_ERR_NO_MEM;
  }

  BaseType_t ok = xTaskCreate(ota_task, "deskmon_ota", 8192, copy, 5, NULL);
  if (ok != pdPASS) {
    free(copy);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}
