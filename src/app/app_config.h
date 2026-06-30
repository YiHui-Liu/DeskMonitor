#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DESKMON_CONFIG_WIFI_SSID_MAX_LEN 32
#define DESKMON_CONFIG_WIFI_PASSWORD_MAX_LEN 64
#define DESKMON_CONFIG_QWEATHER_KEY_MAX_LEN 96
#define DESKMON_CONFIG_QWEATHER_LOCATION_MAX_LEN 32
#define DESKMON_CONFIG_PAGE_NOTE_MAX_LEN 128

#define DESKMON_CONFIG_DEFAULT_CAROUSEL_SEC 15U
#define DESKMON_CONFIG_MIN_CAROUSEL_SEC 5U
#define DESKMON_CONFIG_MAX_CAROUSEL_SEC 3600U

#define DESKMON_CONFIG_DEFAULT_SENSOR_READ_SEC 30U
#define DESKMON_CONFIG_MIN_SENSOR_READ_SEC 1U
#define DESKMON_CONFIG_MAX_SENSOR_READ_SEC 3600U
#define DESKMON_CONFIG_DEFAULT_SENSOR_HISTORY_HOURS 24U
#define DESKMON_CONFIG_MIN_SENSOR_HISTORY_HOURS 1U
#define DESKMON_CONFIG_MAX_SENSOR_HISTORY_HOURS 720U

typedef enum {
    DESKMON_CONFIG_OK = 0,
    DESKMON_CONFIG_ERR_NULL,
    DESKMON_CONFIG_ERR_WIFI_SSID,
    DESKMON_CONFIG_ERR_WIFI_PASSWORD,
    DESKMON_CONFIG_ERR_QWEATHER_LOCATION,
    DESKMON_CONFIG_ERR_CAROUSEL_INTERVAL,
    DESKMON_CONFIG_ERR_SENSOR_READ_INTERVAL,
    DESKMON_CONFIG_ERR_SENSOR_HISTORY_RETENTION,
    DESKMON_CONFIG_ERR_NO_PAGE_ENABLED,
} deskmon_config_status_t;

typedef struct {
    bool summary;
    bool weather;
    bool sensor;
    bool memo;
    bool album;
} deskmon_page_enable_t;

typedef struct {
    char wifi_ssid[DESKMON_CONFIG_WIFI_SSID_MAX_LEN + 1];
    char wifi_password[DESKMON_CONFIG_WIFI_PASSWORD_MAX_LEN + 1];
    char qweather_api_key[DESKMON_CONFIG_QWEATHER_KEY_MAX_LEN + 1];
    char qweather_location[DESKMON_CONFIG_QWEATHER_LOCATION_MAX_LEN + 1];
    char page_summary_note[DESKMON_CONFIG_PAGE_NOTE_MAX_LEN + 1];
    uint32_t carousel_interval_sec;
    uint32_t sensor_read_interval_sec;
    uint32_t sensor_history_retention_hours;
    deskmon_page_enable_t enabled_pages;
} deskmon_config_t;

void deskmon_config_set_defaults(deskmon_config_t *config);
deskmon_config_status_t deskmon_config_validate(const deskmon_config_t *config);
const char *deskmon_config_status_name(deskmon_config_status_t status);

#ifndef UNIT_TEST
#include "esp_err.h"

esp_err_t deskmon_config_load(deskmon_config_t *config);
esp_err_t deskmon_config_save(const deskmon_config_t *config);
#endif
