#include "app/app_storage.h"

#include <esp_err.h>
#include <esp_littlefs.h>
#include <esp_log.h>

static const char *TAG = "deskmon_storage";

esp_err_t deskmon_storage_mount(void) {
  esp_vfs_littlefs_conf_t config = {
      .base_path = DESKMON_STORAGE_BASE_PATH,
      .partition_label = "littlefs",
      .format_if_mount_failed = true,
      .dont_mount = false,
  };

  esp_err_t err = esp_vfs_littlefs_register(&config);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "LittleFS mounted at %s", DESKMON_STORAGE_BASE_PATH);
  }
  return err;
}
