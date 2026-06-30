#include "app/app_config_json.h"

#include <stdbool.h>
#include <string.h>

#include "cJSON.h"

static void copy_json_string(cJSON *root, const char *key, char *target, size_t target_size) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (cJSON_IsString(item) && item->valuestring != NULL) {
    strlcpy(target, item->valuestring, target_size);
  }
}

static void add_pages(cJSON *root, const deskmon_page_enable_t *pages) {
  cJSON *json_pages = cJSON_AddObjectToObject(root, "pages");
  cJSON_AddBoolToObject(json_pages, "summary", pages->summary);
  cJSON_AddBoolToObject(json_pages, "weather", pages->weather);
  cJSON_AddBoolToObject(json_pages, "sensor", pages->sensor);
  cJSON_AddBoolToObject(json_pages, "memo", pages->memo);
  cJSON_AddBoolToObject(json_pages, "album", pages->album);
}

static void read_page_flag(cJSON *pages, const char *key, bool *target) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(pages, key);
  if (cJSON_IsBool(item)) {
    *target = cJSON_IsTrue(item);
  }
}

char *deskmon_config_to_json(const deskmon_config_t *config) {
  if (config == NULL) {
    return NULL;
  }

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return NULL;
  }

  cJSON_AddStringToObject(root, "wifi_ssid", config->wifi_ssid);
  cJSON_AddStringToObject(root, "wifi_password", config->wifi_password);
  cJSON_AddStringToObject(root, "qweather_api_key", config->qweather_api_key);
  cJSON_AddStringToObject(root, "qweather_location", config->qweather_location);
  cJSON_AddStringToObject(root, "page_summary_note", config->page_summary_note);
  cJSON_AddNumberToObject(root, "carousel_interval_sec", config->carousel_interval_sec);
  cJSON_AddNumberToObject(root, "sensor_read_interval_sec", config->sensor_read_interval_sec);
  cJSON_AddNumberToObject(root, "sensor_history_retention_hours", config->sensor_history_retention_hours);
  add_pages(root, &config->enabled_pages);

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json;
}

deskmon_config_status_t deskmon_config_from_json(const char *json, deskmon_config_t *config) {
  if (json == NULL || config == NULL) {
    return DESKMON_CONFIG_ERR_NULL;
  }

  deskmon_config_t next = *config;
  cJSON *root = cJSON_Parse(json);
  if (root == NULL) {
    return DESKMON_CONFIG_ERR_NULL;
  }

  copy_json_string(root, "wifi_ssid", next.wifi_ssid, sizeof(next.wifi_ssid));
  copy_json_string(root, "wifi_password", next.wifi_password, sizeof(next.wifi_password));
  copy_json_string(root, "qweather_api_key", next.qweather_api_key, sizeof(next.qweather_api_key));
  copy_json_string(root, "qweather_location", next.qweather_location, sizeof(next.qweather_location));
  copy_json_string(root, "page_summary_note", next.page_summary_note, sizeof(next.page_summary_note));

  cJSON *carousel = cJSON_GetObjectItemCaseSensitive(root, "carousel_interval_sec");
  if (cJSON_IsNumber(carousel)) {
    next.carousel_interval_sec = (uint32_t)carousel->valuedouble;
  }

  cJSON *sensor_interval = cJSON_GetObjectItemCaseSensitive(root, "sensor_read_interval_sec");
  if (cJSON_IsNumber(sensor_interval)) {
    next.sensor_read_interval_sec = (uint32_t)sensor_interval->valuedouble;
  }

  cJSON *sensor_history = cJSON_GetObjectItemCaseSensitive(root, "sensor_history_retention_hours");
  if (cJSON_IsNumber(sensor_history)) {
    next.sensor_history_retention_hours = (uint32_t)sensor_history->valuedouble;
  }

  cJSON *pages = cJSON_GetObjectItemCaseSensitive(root, "pages");
  if (cJSON_IsObject(pages)) {
    read_page_flag(pages, "summary", &next.enabled_pages.summary);
    read_page_flag(pages, "weather", &next.enabled_pages.weather);
    read_page_flag(pages, "sensor", &next.enabled_pages.sensor);
    read_page_flag(pages, "memo", &next.enabled_pages.memo);
    read_page_flag(pages, "album", &next.enabled_pages.album);
  }

  cJSON_Delete(root);

  deskmon_config_status_t status = deskmon_config_validate(&next);
  if (status == DESKMON_CONFIG_OK) {
    *config = next;
  }
  return status;
}
