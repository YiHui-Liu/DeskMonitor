#include "app/app_display_data.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "sensors/aht20.h"
#include "sensors/ens160.h"
#include "sensors/tsl2591.h"

static const char *TAG = "deskmon_display_data";
static const size_t QWEATHER_RESPONSE_MAX = 8192;
static const uint32_t HISTORY_SIZE = 8;
static const TickType_t FIRST_UPDATE_DELAY = pdMS_TO_TICKS(1500);

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} http_response_t;

typedef struct {
  float values[8];
  uint32_t count;
  uint32_t next;
} value_history_t;

static SemaphoreHandle_t s_lock;
static deskmon_display_snapshot_t s_snapshot;
static const deskmon_config_t *s_config;
static value_history_t s_history[DESKMON_DISPLAY_SENSOR_COUNT];
static bool s_started;

static esp_err_t http_event(esp_http_client_event_t *evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA) {
    return ESP_OK;
  }
  http_response_t *response = evt->user_data;
  if (response == NULL || evt->data == NULL || evt->data_len <= 0) {
    return ESP_OK;
  }
  if (response->length + (size_t)evt->data_len >= response->capacity) {
    return ESP_ERR_NO_MEM;
  }
  memcpy(response->data + response->length, evt->data, evt->data_len);
  response->length += (size_t)evt->data_len;
  response->data[response->length] = '\0';
  return ESP_OK;
}

static esp_err_t fetch_qweather_path(const char *api_key, const char *location, const char *path, char **body) {
  char url[176];
  int written = snprintf(url, sizeof(url), "https://devapi.qweather.com/v7/weather/%s?location=%s", path, location);
  if (written < 0 || (size_t)written >= sizeof(url)) {
    return ESP_ERR_INVALID_SIZE;
  }

  http_response_t response = {
      .data = calloc(QWEATHER_RESPONSE_MAX, 1),
      .capacity = QWEATHER_RESPONSE_MAX,
  };
  if (response.data == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_http_client_config_t client_config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 4000,
      .event_handler = http_event,
      .user_data = &response,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&client_config);
  if (client == NULL) {
    free(response.data);
    return ESP_FAIL;
  }

  esp_http_client_set_header(client, "X-QW-Api-Key", api_key);
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK && esp_http_client_get_status_code(client) >= 200 && esp_http_client_get_status_code(client) < 300) {
    *body = response.data;
  } else {
    free(response.data);
    err = err == ESP_OK ? ESP_FAIL : err;
  }
  esp_http_client_cleanup(client);
  return err;
}

static void history_add(value_history_t *history, float value) {
  history->values[history->next] = value;
  history->next = (history->next + 1U) % HISTORY_SIZE;
  if (history->count < HISTORY_SIZE) {
    history->count++;
  }
}

static void format_history(char *buffer, size_t size, const value_history_t *history, bool decimal) {
  if (history->count == 0) {
    strlcpy(buffer, "最大 -- | 最小 -- | 平均 --", size);
    return;
  }
  float min = history->values[0];
  float max = history->values[0];
  float sum = 0.0f;
  for (uint32_t i = 0; i < history->count; ++i) {
    const float value = history->values[i];
    min = value < min ? value : min;
    max = value > max ? value : max;
    sum += value;
  }
  const float avg = sum / (float)history->count;
  if (decimal) {
    snprintf(buffer, size, "最大 %.1f | 最小 %.1f | 平均 %.1f", max, min, avg);
  } else {
    snprintf(buffer, size, "最大 %.0f | 最小 %.0f | 平均 %.0f", max, min, avg);
  }
}

static void set_sensor_missing(deskmon_display_sensor_t *sensor) {
  strlcpy(sensor->reading, "n/a", sizeof(sensor->reading));
  strlcpy(sensor->status, "缺失", sizeof(sensor->status));
  strlcpy(sensor->stats, "最大 -- | 最小 -- | 平均 --", sizeof(sensor->stats));
  sensor->valid = false;
}

static void update_sensor_snapshot(deskmon_display_snapshot_t *snapshot) {
  float temperature = 0.0f;
  float humidity = 0.0f;
  if (deskmon_aht20_read(&temperature, &humidity) == ESP_OK) {
    history_add(&s_history[0], temperature);
    history_add(&s_history[1], humidity);
    snprintf(snapshot->sensors[0].reading, sizeof(snapshot->sensors[0].reading), "%.1f°C", temperature);
    snprintf(snapshot->sensors[1].reading, sizeof(snapshot->sensors[1].reading), "%.0f%%", humidity);
    strlcpy(snapshot->sensors[0].status, temperature >= 18.0f && temperature <= 30.0f ? "舒适" : "关注", sizeof(snapshot->sensors[0].status));
    strlcpy(snapshot->sensors[1].status, humidity >= 40.0f && humidity <= 70.0f ? "舒适" : "关注", sizeof(snapshot->sensors[1].status));
    format_history(snapshot->sensors[0].stats, sizeof(snapshot->sensors[0].stats), &s_history[0], false);
    format_history(snapshot->sensors[1].stats, sizeof(snapshot->sensors[1].stats), &s_history[1], false);
    snapshot->sensors[0].valid = true;
    snapshot->sensors[1].valid = true;
    snprintf(snapshot->temperature, sizeof(snapshot->temperature), "%.1f°C", temperature);
    snprintf(snapshot->humidity, sizeof(snapshot->humidity), "%.0f%%", humidity);
  } else {
    set_sensor_missing(&snapshot->sensors[0]);
    set_sensor_missing(&snapshot->sensors[1]);
  }

  float lux = 0.0f;
  if (deskmon_tsl2591_read_lux(&lux) == ESP_OK) {
    history_add(&s_history[2], lux);
    snprintf(snapshot->sensors[2].reading, sizeof(snapshot->sensors[2].reading), "%.0flx", lux);
    strlcpy(snapshot->sensors[2].status, lux >= 100.0f && lux <= 1500.0f ? "适中" : "关注", sizeof(snapshot->sensors[2].status));
    format_history(snapshot->sensors[2].stats, sizeof(snapshot->sensors[2].stats), &s_history[2], false);
    snapshot->sensors[2].valid = true;
  } else {
    set_sensor_missing(&snapshot->sensors[2]);
  }

  deskmon_ens160_reading_t ens = {0};
  if (deskmon_ens160_read(&ens) == ESP_OK) {
    history_add(&s_history[3], (float)ens.eco2_ppm);
    history_add(&s_history[4], (float)ens.tvoc_ppb);
    history_add(&s_history[5], (float)ens.aqi);
    snprintf(snapshot->sensors[3].reading, sizeof(snapshot->sensors[3].reading), "%uppm", ens.eco2_ppm);
    snprintf(snapshot->sensors[4].reading, sizeof(snapshot->sensors[4].reading), "%uppb", ens.tvoc_ppb);
    snprintf(snapshot->sensors[5].reading, sizeof(snapshot->sensors[5].reading), "%u", ens.aqi);
    strlcpy(snapshot->sensors[3].status, ens.eco2_ppm <= 1000 ? "良好" : "关注", sizeof(snapshot->sensors[3].status));
    strlcpy(snapshot->sensors[4].status, ens.tvoc_ppb <= 500 ? "优" : "关注", sizeof(snapshot->sensors[4].status));
    strlcpy(snapshot->sensors[5].status, ens.aqi <= 2 ? "优" : "关注", sizeof(snapshot->sensors[5].status));
    format_history(snapshot->sensors[3].stats, sizeof(snapshot->sensors[3].stats), &s_history[3], false);
    format_history(snapshot->sensors[4].stats, sizeof(snapshot->sensors[4].stats), &s_history[4], false);
    format_history(snapshot->sensors[5].stats, sizeof(snapshot->sensors[5].stats), &s_history[5], false);
    snapshot->sensors[3].valid = true;
    snapshot->sensors[4].valid = true;
    snapshot->sensors[5].valid = true;
  } else {
    set_sensor_missing(&snapshot->sensors[3]);
    set_sensor_missing(&snapshot->sensors[4]);
    set_sensor_missing(&snapshot->sensors[5]);
  }
  snapshot->sensors_valid = snapshot->sensors[0].valid || snapshot->sensors[1].valid || snapshot->sensors[2].valid ||
                            snapshot->sensors[3].valid || snapshot->sensors[4].valid || snapshot->sensors[5].valid;
}

static deskmon_weather_icon_t weather_icon_from_text(const char *text) {
  if (text != NULL && strstr(text, "雨") != NULL) {
    return DESKMON_WEATHER_ICON_RAIN;
  }
  if (text != NULL && (strstr(text, "云") != NULL || strstr(text, "阴") != NULL)) {
    return DESKMON_WEATHER_ICON_CLOUD;
  }
  return DESKMON_WEATHER_ICON_SUN;
}

static void date_to_month_day(const char *date, char *out, size_t out_size) {
  int month = 0;
  int day = 0;
  if (date != NULL && sscanf(date, "%*d-%d-%d", &month, &day) == 2) {
    snprintf(out, out_size, "%d月%d", month, day);
  }
}

static void time_to_hour_min(const char *time_text, char *out, size_t out_size) {
  if (time_text != NULL && strlen(time_text) >= 16) {
    snprintf(out, out_size, "%.5s", time_text + 11);
  }
}

static bool parse_now(deskmon_display_snapshot_t *snapshot, const char *body) {
  cJSON *root = cJSON_Parse(body);
  if (root == NULL) {
    return false;
  }
  cJSON *now = cJSON_GetObjectItemCaseSensitive(root, "now");
  cJSON *temp = cJSON_GetObjectItemCaseSensitive(now, "temp");
  cJSON *text = cJSON_GetObjectItemCaseSensitive(now, "text");
  cJSON *humidity = cJSON_GetObjectItemCaseSensitive(now, "humidity");
  cJSON *wind_dir = cJSON_GetObjectItemCaseSensitive(now, "windDir");
  cJSON *wind_speed = cJSON_GetObjectItemCaseSensitive(now, "windSpeed");
  cJSON *pressure = cJSON_GetObjectItemCaseSensitive(now, "pressure");
  cJSON *feels_like = cJSON_GetObjectItemCaseSensitive(now, "feelsLike");
  if (cJSON_IsString(temp) && cJSON_IsString(text)) {
    snprintf(snapshot->temperature, sizeof(snapshot->temperature), "%s°C", temp->valuestring);
    strlcpy(snapshot->weather_text, text->valuestring, sizeof(snapshot->weather_text));
  }
  if (cJSON_IsString(humidity)) {
    snprintf(snapshot->humidity, sizeof(snapshot->humidity), "%s%%", humidity->valuestring);
  }
  if (cJSON_IsString(wind_dir) && cJSON_IsString(wind_speed)) {
    snprintf(snapshot->wind, sizeof(snapshot->wind), "%s %skm/h", wind_dir->valuestring, wind_speed->valuestring);
  }
  if (cJSON_IsString(pressure)) {
    snprintf(snapshot->pressure, sizeof(snapshot->pressure), "%s hPa", pressure->valuestring);
  }
  if (cJSON_IsString(feels_like)) {
    snprintf(snapshot->feels_like, sizeof(snapshot->feels_like), "%s°C", feels_like->valuestring);
  }
  cJSON_Delete(root);
  return true;
}

static bool parse_hourly(deskmon_display_snapshot_t *snapshot, const char *body) {
  cJSON *root = cJSON_Parse(body);
  if (root == NULL) {
    return false;
  }
  cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
  for (int i = 0; i < DESKMON_DISPLAY_HOURLY_COUNT; ++i) {
    cJSON *item = cJSON_GetArrayItem(hourly, i);
    cJSON *fx_time = cJSON_GetObjectItemCaseSensitive(item, "fxTime");
    cJSON *text = cJSON_GetObjectItemCaseSensitive(item, "text");
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(item, "temp");
    if (cJSON_IsString(fx_time)) {
      time_to_hour_min(fx_time->valuestring, snapshot->hourly[i].time, sizeof(snapshot->hourly[i].time));
    }
    if (cJSON_IsString(text) && cJSON_IsString(temp)) {
      snprintf(snapshot->hourly[i].weather, sizeof(snapshot->hourly[i].weather), "%s %s°", text->valuestring, temp->valuestring);
    }
  }
  cJSON_Delete(root);
  return true;
}

static bool parse_daily(deskmon_display_snapshot_t *snapshot, const char *body) {
  cJSON *root = cJSON_Parse(body);
  if (root == NULL) {
    return false;
  }
  cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
  for (int i = 0; i < DESKMON_DISPLAY_DAILY_COUNT; ++i) {
    cJSON *item = cJSON_GetArrayItem(daily, i);
    cJSON *fx_date = cJSON_GetObjectItemCaseSensitive(item, "fxDate");
    cJSON *text = cJSON_GetObjectItemCaseSensitive(item, "textDay");
    cJSON *temp_min = cJSON_GetObjectItemCaseSensitive(item, "tempMin");
    cJSON *temp_max = cJSON_GetObjectItemCaseSensitive(item, "tempMax");
    cJSON *precip = cJSON_GetObjectItemCaseSensitive(item, "precip");
    if (cJSON_IsString(fx_date)) {
      date_to_month_day(fx_date->valuestring, snapshot->daily[i].date, sizeof(snapshot->daily[i].date));
    }
    if (cJSON_IsString(text)) {
      snapshot->daily[i].icon = weather_icon_from_text(text->valuestring);
    }
    if (cJSON_IsString(temp_min) && cJSON_IsString(temp_max)) {
      snprintf(snapshot->daily[i].temp, sizeof(snapshot->daily[i].temp), "%s~%s°", temp_min->valuestring, temp_max->valuestring);
      if (i == 0) {
        snprintf(snapshot->low, sizeof(snapshot->low), "%s°C", temp_min->valuestring);
        snprintf(snapshot->high, sizeof(snapshot->high), "%s°C", temp_max->valuestring);
      }
    }
    if (i == 0 && cJSON_IsString(precip)) {
      snprintf(snapshot->precip, sizeof(snapshot->precip), "%smm", precip->valuestring);
    }
  }
  cJSON_Delete(root);
  return true;
}

static bool update_weather_snapshot(deskmon_display_snapshot_t *snapshot) {
  if (s_config == NULL || s_config->qweather_api_key[0] == '\0' || s_config->qweather_location[0] == '\0') {
    return false;
  }
  strlcpy(snapshot->location, s_config->qweather_location, sizeof(snapshot->location));
  char *body = NULL;
  bool ok = false;
  if (fetch_qweather_path(s_config->qweather_api_key, s_config->qweather_location, "now", &body) == ESP_OK) {
    ok = parse_now(snapshot, body);
    free(body);
  }
  body = NULL;
  if (fetch_qweather_path(s_config->qweather_api_key, s_config->qweather_location, "24h", &body) == ESP_OK) {
    ok = parse_hourly(snapshot, body) || ok;
    free(body);
  }
  body = NULL;
  if (fetch_qweather_path(s_config->qweather_api_key, s_config->qweather_location, "7d", &body) == ESP_OK) {
    ok = parse_daily(snapshot, body) || ok;
    free(body);
  }
  snapshot->weather_valid = ok;
  return ok;
}

static TickType_t weather_refresh_ticks(void) {
  uint32_t interval_min = DESKMON_CONFIG_DEFAULT_WEATHER_REFRESH_MIN;
  if (s_config != NULL) {
    interval_min = s_config->weather_refresh_interval_min;
  }
  if (interval_min < DESKMON_CONFIG_MIN_WEATHER_REFRESH_MIN) {
    interval_min = DESKMON_CONFIG_MIN_WEATHER_REFRESH_MIN;
  }
  return pdMS_TO_TICKS(interval_min * 60U * 1000U);
}

static void display_data_task(void *arg) {
  (void)arg;
  vTaskDelay(FIRST_UPDATE_DELAY);
  TickType_t last_weather_attempt = 0;
  bool weather_attempted = false;
  while (true) {
    deskmon_display_snapshot_t next;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
      next = s_snapshot;
      xSemaphoreGive(s_lock);
    } else {
      deskmon_display_snapshot_defaults(&next);
    }
    update_sensor_snapshot(&next);
    const TickType_t now = xTaskGetTickCount();
    if (!weather_attempted || now - last_weather_attempt >= weather_refresh_ticks()) {
      deskmon_display_snapshot_t weather_next = next;
      last_weather_attempt = now;
      weather_attempted = true;
      if (update_weather_snapshot(&weather_next)) {
        next = weather_next;
      }
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
      s_snapshot = next;
      xSemaphoreGive(s_lock);
    }
    const uint32_t interval_sec = s_config != NULL ? s_config->sensor_read_interval_sec : DESKMON_CONFIG_DEFAULT_SENSOR_READ_SEC;
    vTaskDelay(pdMS_TO_TICKS(interval_sec * 1000U));
  }
}

esp_err_t deskmon_display_data_start(const deskmon_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  s_config = config;
  if (s_lock == NULL) {
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }
  if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
    deskmon_display_snapshot_defaults(&s_snapshot);
    xSemaphoreGive(s_lock);
  }
  if (s_started) {
    return ESP_OK;
  }
  if (xTaskCreate(display_data_task, "display_data", 8192, NULL, 5, NULL) != pdPASS) {
    return ESP_ERR_NO_MEM;
  }
  s_started = true;
  ESP_LOGI(TAG, "display data updater started");
  return ESP_OK;
}

bool deskmon_display_data_snapshot(deskmon_display_snapshot_t *snapshot, void *ctx) {
  (void)ctx;
  if (snapshot == NULL || s_lock == NULL) {
    return false;
  }
  if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }
  *snapshot = s_snapshot;
  xSemaphoreGive(s_lock);
  return true;
}
