#include "app/app_wifi.h"

#include <string.h>

#include <esp_check.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/inet.h>

static const char *TAG = "deskmon_wifi";
static const int WIFI_CONNECTED_BIT = BIT0;

static EventGroupHandle_t s_wifi_events;
static int s_retry_count;
static deskmon_wifi_status_t s_status;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  const deskmon_config_t *config = arg;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    if (config->wifi_ssid[0] != '\0') {
      esp_wifi_connect();
    }
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    s_status.sta_connected = false;
    s_status.sta_ip[0] = '\0';
    if (config->wifi_ssid[0] != '\0' && s_retry_count < 3) {
      s_retry_count++;
      esp_wifi_connect();
      ESP_LOGW(TAG, "STA disconnected, retry %d", s_retry_count);
    } else {
      ESP_LOGW(TAG, "STA unavailable, setup AP remains active");
    }
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
    s_status.ap_started = true;
    ESP_LOGI(TAG, "setup AP started");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = event_data;
    s_retry_count = 0;
    s_status.sta_connected = true;
    snprintf(s_status.sta_ip, sizeof(s_status.sta_ip), IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG, "STA connected: %s", s_status.sta_ip);
  }
}

esp_err_t deskmon_wifi_start(const deskmon_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  s_wifi_events = xEventGroupCreate();
  if (s_wifi_events == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "wifi init failed");

  ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, (void *)config), TAG,
                      "wifi handler failed");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, (void *)config),
                      TAG, "ip handler failed");

  wifi_config_t ap_config = {
      .ap =
          {
              .ssid = "DeskMonitor-Setup",
              .ssid_len = strlen("DeskMonitor-Setup"),
              .password = "deskmonitor",
              .channel = 1,
              .max_connection = 2,
              .authmode = WIFI_AUTH_WPA2_PSK,
          },
  };

  wifi_config_t sta_config = {0};
  strlcpy((char *)sta_config.sta.ssid, config->wifi_ssid, sizeof(sta_config.sta.ssid));
  strlcpy((char *)sta_config.sta.password, config->wifi_password, sizeof(sta_config.sta.password));
  sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "set APSTA mode failed");
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "set AP config failed");
  if (config->wifi_ssid[0] != '\0') {
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_config), TAG, "set STA config failed");
  }
  ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "disable WiFi power save failed");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

  if (config->wifi_ssid[0] == '\0') {
    ESP_LOGI(TAG, "no STA credentials, setup AP is the active path");
  }
  return ESP_OK;
}

deskmon_wifi_status_t deskmon_wifi_status(void) { return s_status; }
