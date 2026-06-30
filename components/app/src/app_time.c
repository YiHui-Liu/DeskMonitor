#include "app/app_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <cJSON.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>

static const char *TAG = "deskmon_time";
static const int DESKMON_TIME_VALID_YEAR = 2024;

static bool s_sntp_initialized;
static esp_err_t s_last_apply_err = ESP_ERR_INVALID_STATE;
static char s_ntp_server[DESKMON_CONFIG_NTP_SERVER_MAX_LEN + 1];
static char s_timezone[DESKMON_CONFIG_TIMEZONE_MAX_LEN + 1];

static bool current_time_is_valid(time_t now) {
  struct tm utc;
  if (gmtime_r(&now, &utc) == NULL) {
    return false;
  }
  return utc.tm_year + 1900 >= DESKMON_TIME_VALID_YEAR;
}

static void format_time(char *buffer, size_t buffer_size, const char *format, const struct tm *time_info) {
  if (strftime(buffer, buffer_size, format, time_info) == 0) {
    buffer[0] = '\0';
  }
}

cJSON *create_time_status_json(void) {
  struct timeval tv = {0};
  gettimeofday(&tv, NULL);

  time_t now = tv.tv_sec;
  struct tm utc;
  struct tm local;
  gmtime_r(&now, &utc);
  localtime_r(&now, &local);

  char utc_text[32];
  char local_text[40];
  format_time(utc_text, sizeof(utc_text), "%Y-%m-%dT%H:%M:%SZ", &utc);
  format_time(local_text, sizeof(local_text), "%Y-%m-%dT%H:%M:%S%z", &local);

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return NULL;
  }

  cJSON_AddNumberToObject(root, "epoch_sec", (double)tv.tv_sec);
  cJSON_AddNumberToObject(root, "epoch_usec", tv.tv_usec);
  cJSON_AddBoolToObject(root, "time_valid", current_time_is_valid(now));
  cJSON_AddStringToObject(root, "utc_time", utc_text);
  cJSON_AddStringToObject(root, "local_time", local_text);
  cJSON_AddStringToObject(root, "timezone", s_timezone);
  cJSON_AddStringToObject(root, "ntp_server", s_ntp_server);
  cJSON_AddBoolToObject(root, "sntp_started", s_sntp_initialized && s_last_apply_err == ESP_OK);
  cJSON_AddStringToObject(root, "sntp_status", esp_err_to_name(s_last_apply_err));
  return root;
}

esp_err_t deskmon_time_apply_config(const deskmon_config_t *config) {
  deskmon_config_status_t status = deskmon_config_validate(config);
  if (status != DESKMON_CONFIG_OK) {
    ESP_LOGW(TAG, "time config rejected: %s", deskmon_config_status_name(status));
    s_last_apply_err = ESP_ERR_INVALID_ARG;
    return ESP_ERR_INVALID_ARG;
  }

  if (setenv("TZ", config->timezone, 1) != 0) {
    s_last_apply_err = ESP_FAIL;
    return ESP_FAIL;
  }
  tzset();

  strlcpy(s_ntp_server, config->ntp_server, sizeof(s_ntp_server));
  strlcpy(s_timezone, config->timezone, sizeof(s_timezone));

  if (s_sntp_initialized) {
    esp_netif_sntp_deinit();
    s_sntp_initialized = false;
  }

  esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(config->ntp_server);
  sntp_config.start = false;

  esp_err_t err = esp_netif_sntp_init(&sntp_config);
  if (err != ESP_OK) {
    s_last_apply_err = err;
    return err;
  }
  s_sntp_initialized = true;

  err = esp_netif_sntp_start();
  s_last_apply_err = err;
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "SNTP started: server=%s timezone=%s", s_ntp_server, s_timezone);
  }
  return err;
}

esp_err_t deskmon_time_set_epoch(int64_t epoch_sec) {
  if (epoch_sec < 0) {
    return ESP_ERR_INVALID_ARG;
  }

  struct timeval tv = {
      .tv_sec = (time_t)epoch_sec,
      .tv_usec = 0,
  };
  if (settimeofday(&tv, NULL) != 0) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

char *deskmon_time_json(void) {
  cJSON *root = create_time_status_json();
  if (root == NULL) {
    return NULL;
  }

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json;
}