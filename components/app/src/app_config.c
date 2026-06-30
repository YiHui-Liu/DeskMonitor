#include "app/app_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <esp_err.h>
#include <esp_log.h>

#include "app/app_config_json.h"
#include "app/app_storage.h"

static const char *TAG = "deskmon_config";

static esp_err_t use_default_config(deskmon_config_t *config, const char *reason) {
  deskmon_config_set_defaults(config);
  ESP_LOGW(TAG, "%s, using defaults", reason);
  return ESP_OK;
}

esp_err_t deskmon_config_load(deskmon_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  FILE *file = fopen(DESKMON_CONFIG_FILE_PATH, "r");
  if (file == NULL) {
    return use_default_config(config, "config not found");
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return use_default_config(config, "config size check failed");
  }
  long size = ftell(file);
  rewind(file);

  if (size <= 0 || size > 2048) {
    fclose(file);
    return use_default_config(config, "config file size invalid");
  }

  char *json = calloc((size_t)size + 1, 1);
  if (json == NULL) {
    fclose(file);
    return ESP_ERR_NO_MEM;
  }

  size_t read_size = fread(json, 1, (size_t)size, file);
  fclose(file);

  if (read_size != (size_t)size) {
    free(json);
    return use_default_config(config, "config read failed");
  }

  deskmon_config_status_t status = deskmon_config_from_json(json, config);
  free(json);

  if (status != DESKMON_CONFIG_OK) {
    return use_default_config(config, deskmon_config_status_name(status));
  }
  return ESP_OK;
}

esp_err_t deskmon_config_save(const deskmon_config_t *config) {
  deskmon_config_status_t status = deskmon_config_validate(config);
  if (status != DESKMON_CONFIG_OK) {
    ESP_LOGW(TAG, "config rejected: %s", deskmon_config_status_name(status));
    return ESP_ERR_INVALID_ARG;
  }

  char *json = deskmon_config_to_json(config);
  if (json == NULL) {
    return ESP_ERR_NO_MEM;
  }

  const char *tmp_path = DESKMON_CONFIG_FILE_PATH ".tmp";
  FILE *file = fopen(tmp_path, "w");
  if (file == NULL) {
    free(json);
    return ESP_FAIL;
  }

  int written = fputs(json, file);
  int close_status = fclose(file);
  free(json);

  if (written < 0 || close_status != 0) {
    unlink(tmp_path);
    return ESP_FAIL;
  }

  if (rename(tmp_path, DESKMON_CONFIG_FILE_PATH) != 0) {
    unlink(tmp_path);
    return ESP_FAIL;
  }

  return ESP_OK;
}
