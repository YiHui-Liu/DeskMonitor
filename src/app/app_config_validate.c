#include "app/app_config.h"

#include <string.h>

static bool string_is_terminated(const char *value, size_t capacity) { return memchr(value, '\0', capacity) != NULL; }

static bool any_page_enabled(const deskmon_page_enable_t *pages) {
  return pages->summary || pages->weather || pages->sensor || pages->memo || pages->album;
}

void deskmon_config_set_defaults(deskmon_config_t *config) {
  if (config == NULL) {
    return;
  }

  memset(config, 0, sizeof(*config));
  strcpy(config->qweather_location, "101010100");
  strcpy(config->page_summary_note, "Desk Monitor / 桌面监视器");
  config->carousel_interval_sec = DESKMON_CONFIG_DEFAULT_CAROUSEL_SEC;
  config->sensor_read_interval_sec = DESKMON_CONFIG_DEFAULT_SENSOR_READ_SEC;
  config->sensor_history_retention_hours = DESKMON_CONFIG_DEFAULT_SENSOR_HISTORY_HOURS;
  config->enabled_pages.summary = true;
  config->enabled_pages.weather = true;
  config->enabled_pages.sensor = true;
  config->enabled_pages.memo = true;
  config->enabled_pages.album = true;
}

deskmon_config_status_t deskmon_config_validate(const deskmon_config_t *config) {
  if (config == NULL) {
    return DESKMON_CONFIG_ERR_NULL;
  }

  if (!string_is_terminated(config->wifi_ssid, sizeof(config->wifi_ssid))) {
    return DESKMON_CONFIG_ERR_WIFI_SSID;
  }

  if (!string_is_terminated(config->wifi_password, sizeof(config->wifi_password))) {
    return DESKMON_CONFIG_ERR_WIFI_PASSWORD;
  }

  size_t ssid_len = strlen(config->wifi_ssid);
  size_t password_len = strlen(config->wifi_password);
  if (ssid_len > DESKMON_CONFIG_WIFI_SSID_MAX_LEN) {
    return DESKMON_CONFIG_ERR_WIFI_SSID;
  }

  if (password_len > 0 && password_len < 8) {
    return DESKMON_CONFIG_ERR_WIFI_PASSWORD;
  }

  if (!string_is_terminated(config->qweather_api_key, sizeof(config->qweather_api_key))) {
    return DESKMON_CONFIG_ERR_QWEATHER_LOCATION;
  }

  if (!string_is_terminated(config->qweather_location, sizeof(config->qweather_location))) {
    return DESKMON_CONFIG_ERR_QWEATHER_LOCATION;
  }

  if (!string_is_terminated(config->page_summary_note, sizeof(config->page_summary_note))) {
    return DESKMON_CONFIG_ERR_QWEATHER_LOCATION;
  }

  if (config->carousel_interval_sec < DESKMON_CONFIG_MIN_CAROUSEL_SEC ||
      config->carousel_interval_sec > DESKMON_CONFIG_MAX_CAROUSEL_SEC) {
    return DESKMON_CONFIG_ERR_CAROUSEL_INTERVAL;
  }

  if (config->sensor_read_interval_sec < DESKMON_CONFIG_MIN_SENSOR_READ_SEC ||
      config->sensor_read_interval_sec > DESKMON_CONFIG_MAX_SENSOR_READ_SEC) {
    return DESKMON_CONFIG_ERR_SENSOR_READ_INTERVAL;
  }

  if (config->sensor_history_retention_hours < DESKMON_CONFIG_MIN_SENSOR_HISTORY_HOURS ||
      config->sensor_history_retention_hours > DESKMON_CONFIG_MAX_SENSOR_HISTORY_HOURS) {
    return DESKMON_CONFIG_ERR_SENSOR_HISTORY_RETENTION;
  }

  if (!any_page_enabled(&config->enabled_pages)) {
    return DESKMON_CONFIG_ERR_NO_PAGE_ENABLED;
  }

  return DESKMON_CONFIG_OK;
}

const char *deskmon_config_status_name(deskmon_config_status_t status) {
  switch (status) {
  case DESKMON_CONFIG_OK:
    return "ok";
  case DESKMON_CONFIG_ERR_NULL:
    return "null config";
  case DESKMON_CONFIG_ERR_WIFI_SSID:
    return "invalid wifi ssid";
  case DESKMON_CONFIG_ERR_WIFI_PASSWORD:
    return "invalid wifi password";
  case DESKMON_CONFIG_ERR_QWEATHER_LOCATION:
    return "invalid qweather field";
  case DESKMON_CONFIG_ERR_CAROUSEL_INTERVAL:
    return "invalid carousel interval";
  case DESKMON_CONFIG_ERR_SENSOR_READ_INTERVAL:
    return "invalid sensor read interval";
  case DESKMON_CONFIG_ERR_SENSOR_HISTORY_RETENTION:
    return "invalid sensor history retention";
  case DESKMON_CONFIG_ERR_NO_PAGE_ENABLED:
    return "no page enabled";
  default:
    return "unknown config status";
  }
}
