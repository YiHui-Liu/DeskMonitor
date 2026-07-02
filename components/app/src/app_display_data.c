#include "app/app_display_data.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>

#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "app/app_gzip.h"
#include "sensors/aht20.h"
#include "sensors/ens160.h"
#include "sensors/tsl2591.h"

static const char *TAG = "deskmon_display_data";
static const size_t QWEATHER_RESPONSE_MAX = 8192;
static const size_t QWEATHER_DECOMPRESSED_MAX = 12288;
static const TickType_t FIRST_UPDATE_DELAY = pdMS_TO_TICKS(1500);
static const TickType_t WEATHER_RETRY_DELAY = pdMS_TO_TICKS(60 * 1000);
static const size_t WEATHER_FETCH_TASK_STACK = 16384;

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} http_response_t;

typedef struct {
  float values[DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT];
  uint32_t count;
  uint32_t next;
} value_history_t;

static SemaphoreHandle_t s_lock;
static deskmon_display_snapshot_t s_snapshot;
static const deskmon_config_t *s_config;
static value_history_t s_history[DESKMON_DISPLAY_SENSOR_COUNT];
static bool s_started;
static bool s_wifi_connected;
static bool s_weather_fetch_active;
static TickType_t s_last_weather_attempt;

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
  esp_http_client_set_header(client, "Accept-Encoding", "identity");
  esp_err_t err = esp_http_client_perform(client);
  const int status = err == ESP_OK ? esp_http_client_get_status_code(client) : -1;
  if (err == ESP_OK && status >= 200 && status < 300) {
    if (response.length >= 2 && (uint8_t)response.data[0] == 0x1f && (uint8_t)response.data[1] == 0x8b) {
      uint8_t *dec = calloc(QWEATHER_DECOMPRESSED_MAX, 1);
      size_t dec_len = 0;
      if (dec != NULL && deskmon_gunzip((uint8_t *)response.data, response.length, dec, QWEATHER_DECOMPRESSED_MAX,
                                        &dec_len) == ESP_OK) {
        dec[dec_len] = '\0';
        free(response.data);
        *body = (char *)dec;
      } else {
        free(dec);
        free(response.data);
        ESP_LOGW(TAG, "qweather %s gzip decompress failed, bytes=%u", path, (unsigned)response.length);
        err = ESP_FAIL;
      }
    } else {
      *body = response.data;
    }
  } else {
    ESP_LOGW(TAG, "qweather %s failed: err=%s http=%d bytes=%u", path, esp_err_to_name(err), status,
             (unsigned)response.length);
    free(response.data);
    err = err == ESP_OK ? ESP_FAIL : err;
  }
  esp_http_client_cleanup(client);
  return err;
}

static void history_add(value_history_t *history, float value) {
  history->values[history->next] = value;
  history->next = (history->next + 1U) % DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT;
  if (history->count < DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT) {
    history->count++;
  }
}

static void format_history(char *buffer, size_t size, const value_history_t *history, bool decimal) {
  if (history->count == 0) {
    strlcpy(buffer, "Max ----- | Min -----", size);
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
  if (decimal) {
    snprintf(buffer, size, "Max %5.1f | Min %5.1f", max, min);
  } else {
    snprintf(buffer, size, "Max %5.0f | Min %5.0f", max, min);
  }
}

static void copy_history_samples(deskmon_display_sensor_t *sensor, const value_history_t *history) {
  const uint32_t count = history->count;
  sensor->sample_count = count;
  const uint32_t oldest = count == DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT ? history->next : 0;
  for (uint32_t i = 0; i < count; ++i) {
    sensor->samples[i] = history->values[(oldest + i) % DESKMON_DISPLAY_SENSOR_SAMPLE_COUNT];
  }
}

static float apparent_temperature(float t, float rh) {
  float vapor = rh / 100.0f * 6.105f * expf(17.27f * t / (237.7f + t));
  return 1.07f * t + 0.2f * vapor - 2.7f;
}

static const char *temperature_status(float at) {
  if (at < 0.f)
    return "严寒";
  else if (at < 15.f)
    return "寒冷";
  else if (at < 20.f)
    return "凉爽";
  else if (at < 26.f)
    return "舒适";
  else if (at < 30.f)
    return "微热";
  else if (at < 35.f)
    return "炎热";
  else
    return "酷热";
}

static const char *humidity_status(float rh) {
  if (rh < 20.0f)
    return "过干";
  else if (rh < 40.0f)
    return "干燥";
  else if (rh <= 60.0f)
    return "适宜";
  else if (rh <= 80.0f)
    return "潮湿";
  else
    return "过湿";
}

static const char *light_status(float light) {
  if (light < 50.0f)
    return "过暗";
  if (light < 150.0f)
    return "偏暗";
  if (light <= 1000.0f)
    return "适宜";
  if (light <= 2000.0f)
    return "偏亮";
  return "过亮";
}

static const char *status_five_level(uint32_t level) {
  static const char *const labels[] = {"优秀", "良好", "一般", "较差", "极差"};
  return labels[level < 5U ? level : 4U];
}

static const char *co2_status(uint16_t eco2) {
  if (eco2 <= 600U)
    return status_five_level(0);
  if (eco2 <= 800U)
    return status_five_level(1);
  if (eco2 <= 1000U)
    return status_five_level(2);
  if (eco2 <= 1500U)
    return status_five_level(3);
  return status_five_level(4);
}

static const char *tvoc_status(uint16_t tvoc) {
  if (tvoc < 65U)
    return status_five_level(0);
  if (tvoc < 220U)
    return status_five_level(1);
  if (tvoc < 650U)
    return status_five_level(2);
  if (tvoc < 2200U)
    return status_five_level(3);
  return status_five_level(4);
}

static const char *aqi_status(uint8_t aqi) { return status_five_level(aqi > 0U ? (uint32_t)aqi - 1U : 4U); }

static void set_sensor_missing(deskmon_display_sensor_t *sensor) {
  strlcpy(sensor->reading, "n/a", sizeof(sensor->reading));
  strlcpy(sensor->status, "", sizeof(sensor->status));
  strlcpy(sensor->stats, "Max ----- | Min -----", sizeof(sensor->stats));
  sensor->sample_count = 0;
  sensor->valid = false;
}

static void update_sensor_snapshot(deskmon_display_snapshot_t *snapshot) {
  float temperature = 0.0f;
  float humidity = 0.0f;
  if (deskmon_aht20_read(&temperature, &humidity) == ESP_OK) {
    float at = apparent_temperature(temperature, humidity);

    history_add(&s_history[0], temperature);
    history_add(&s_history[1], humidity);
    snprintf(snapshot->sensors[0].reading, sizeof(snapshot->sensors[0].reading), "%.1f°C", temperature);
    snprintf(snapshot->sensors[1].reading, sizeof(snapshot->sensors[1].reading), "%.0f%%", humidity);
    strlcpy(snapshot->sensors[0].status, temperature_status(at), sizeof(snapshot->sensors[0].status));
    strlcpy(snapshot->sensors[1].status, humidity_status(humidity), sizeof(snapshot->sensors[1].status));
    format_history(snapshot->sensors[0].stats, sizeof(snapshot->sensors[0].stats), &s_history[0], true);
    format_history(snapshot->sensors[1].stats, sizeof(snapshot->sensors[1].stats), &s_history[1], true);
    copy_history_samples(&snapshot->sensors[0], &s_history[0]);
    copy_history_samples(&snapshot->sensors[1], &s_history[1]);
    snapshot->sensors[0].valid = true;
    snapshot->sensors[1].valid = true;
  } else {
    set_sensor_missing(&snapshot->sensors[0]);
    set_sensor_missing(&snapshot->sensors[1]);
  }

  float lux = 0.0f;
  if (deskmon_tsl2591_read_lux(&lux) == ESP_OK) {
    history_add(&s_history[2], lux);
    snprintf(snapshot->sensors[2].reading, sizeof(snapshot->sensors[2].reading), "%.0flx", lux);
    strlcpy(snapshot->sensors[2].status, light_status(lux), sizeof(snapshot->sensors[2].status));
    format_history(snapshot->sensors[2].stats, sizeof(snapshot->sensors[2].stats), &s_history[2], false);
    copy_history_samples(&snapshot->sensors[2], &s_history[2]);
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
    strlcpy(snapshot->sensors[3].status, co2_status(ens.eco2_ppm), sizeof(snapshot->sensors[3].status));
    strlcpy(snapshot->sensors[4].status, tvoc_status(ens.tvoc_ppb), sizeof(snapshot->sensors[4].status));
    strlcpy(snapshot->sensors[5].status, aqi_status(ens.aqi), sizeof(snapshot->sensors[5].status));
    format_history(snapshot->sensors[3].stats, sizeof(snapshot->sensors[3].stats), &s_history[3], false);
    format_history(snapshot->sensors[4].stats, sizeof(snapshot->sensors[4].stats), &s_history[4], false);
    format_history(snapshot->sensors[5].stats, sizeof(snapshot->sensors[5].stats), &s_history[5], false);
    copy_history_samples(&snapshot->sensors[3], &s_history[3]);
    copy_history_samples(&snapshot->sensors[4], &s_history[4]);
    copy_history_samples(&snapshot->sensors[5], &s_history[5]);
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

  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm utc;
  gmtime_r(&tv.tv_sec, &utc);
  if (utc.tm_year + 1900 >= 2024) {
    struct tm local;
    localtime_r(&tv.tv_sec, &local);
    strftime(snapshot->date, sizeof(snapshot->date), "%Y-%m-%d", &local);
    strftime(snapshot->time, sizeof(snapshot->time), "%H:%M", &local);
    static const char *const wday_cn[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    strlcpy(snapshot->weekday, wday_cn[local.tm_wday % 7], sizeof(snapshot->weekday));
  }
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
    snprintf(out, out_size, "%d月%d日", month, day);
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
    snprintf(snapshot->weather_temperature, sizeof(snapshot->weather_temperature), "%s°C", temp->valuestring);
    strlcpy(snapshot->weather_text, text->valuestring, sizeof(snapshot->weather_text));
  }
  if (cJSON_IsString(humidity)) {
    snprintf(snapshot->weather_humidity, sizeof(snapshot->weather_humidity), "%s%%", humidity->valuestring);
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
      snprintf(snapshot->hourly[i].weather, sizeof(snapshot->hourly[i].weather), "%s %s°", text->valuestring,
               temp->valuestring);
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
      snprintf(snapshot->daily[i].low, sizeof(snapshot->daily[i].low), "%s°C", temp_min->valuestring);
      snprintf(snapshot->daily[i].high, sizeof(snapshot->daily[i].high), "%s°C", temp_max->valuestring);
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

static void copy_weather_fields(deskmon_display_snapshot_t *dst, const deskmon_display_snapshot_t *src) {
  strlcpy(dst->location, src->location, sizeof(dst->location));
  strlcpy(dst->weather_text, src->weather_text, sizeof(dst->weather_text));
  strlcpy(dst->weather_temperature, src->weather_temperature, sizeof(dst->weather_temperature));
  strlcpy(dst->weather_humidity, src->weather_humidity, sizeof(dst->weather_humidity));
  strlcpy(dst->wind, src->wind, sizeof(dst->wind));
  strlcpy(dst->high, src->high, sizeof(dst->high));
  strlcpy(dst->low, src->low, sizeof(dst->low));
  strlcpy(dst->pressure, src->pressure, sizeof(dst->pressure));
  strlcpy(dst->feels_like, src->feels_like, sizeof(dst->feels_like));
  strlcpy(dst->precip, src->precip, sizeof(dst->precip));
  memcpy(dst->hourly, src->hourly, sizeof(dst->hourly));
  memcpy(dst->daily, src->daily, sizeof(dst->daily));
  dst->weather_valid = src->weather_valid;
}

static void weather_fetch_task(void *arg) {
  (void)arg;
  vTaskDelay(pdMS_TO_TICKS(500));

  deskmon_display_snapshot_t draft;
  if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
    draft = s_snapshot;
    xSemaphoreGive(s_lock);
  } else {
    draft = (deskmon_display_snapshot_t){0};
  }

  bool ok = update_weather_snapshot(&draft);

  if (ok) {
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
      copy_weather_fields(&s_snapshot, &draft);
      xSemaphoreGive(s_lock);
    }
    ESP_LOGI(TAG, "weather fetch succeeded");
  } else {
    s_last_weather_attempt = xTaskGetTickCount() - weather_refresh_ticks() + WEATHER_RETRY_DELAY;
    ESP_LOGW(TAG, "weather fetch failed, keeping last data");
  }

  if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
    s_weather_fetch_active = false;
    xSemaphoreGive(s_lock);
  }
  vTaskDelete(NULL);
}

static void request_weather_refresh(void) {
  if (s_config == NULL || s_config->qweather_api_key[0] == '\0' || s_config->qweather_location[0] == '\0') {
    return;
  }

  bool spawn = false;
  if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
    if (!s_weather_fetch_active) {
      s_weather_fetch_active = true;
      spawn = true;
    }
    xSemaphoreGive(s_lock);
  }
  if (!spawn) {
    return;
  }

  s_last_weather_attempt = xTaskGetTickCount();

  if (xTaskCreate(weather_fetch_task, "wfetch", WEATHER_FETCH_TASK_STACK, NULL, 5, NULL) != pdPASS) {
    ESP_LOGW(TAG, "weather fetch task create failed");
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
      s_weather_fetch_active = false;
      xSemaphoreGive(s_lock);
    }
  }
}

static void display_data_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  (void)event_data;
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    s_wifi_connected = true;
    ESP_LOGI(TAG, "WiFi connected, triggering weather fetch");
    request_weather_refresh();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    s_wifi_connected = false;
  }
}

static void display_data_task(void *arg) {
  (void)arg;
  vTaskDelay(FIRST_UPDATE_DELAY);

  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    s_wifi_connected = (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0);
  }

  while (true) {
    deskmon_display_snapshot_t next;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
      next = s_snapshot;
      xSemaphoreGive(s_lock);
    }
    update_sensor_snapshot(&next);
    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
      s_snapshot = next;
      xSemaphoreGive(s_lock);
    }

    const TickType_t now = xTaskGetTickCount();
    if (s_wifi_connected && (s_last_weather_attempt == 0 || now - s_last_weather_attempt >= weather_refresh_ticks())) {
      request_weather_refresh();
    }

    const uint32_t interval_sec =
        s_config != NULL ? s_config->sensor_read_interval_sec : DESKMON_CONFIG_DEFAULT_SENSOR_READ_SEC;
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
    deskmon_display_snapshot_init(&s_snapshot);
    strlcpy(s_snapshot.location, config->qweather_location, sizeof(s_snapshot.location));
    xSemaphoreGive(s_lock);
  }
  if (s_started) {
    return ESP_OK;
  }

  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, display_data_event_handler, NULL);
  esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, display_data_event_handler, NULL);

  if (xTaskCreate(display_data_task, "display_data", 4096, NULL, 5, NULL) != pdPASS) {
    return ESP_ERR_NO_MEM;
  }
  s_started = true;
  ESP_LOGI(TAG, "display data updater started (4 KB task; weather TLS + gzip in on-demand 16 KB wfetch task)");
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
