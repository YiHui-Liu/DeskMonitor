#include <assert.h>
#include <string.h>

#include "app/app_config.h"

static void test_defaults_are_safe_for_first_boot(void)
{
    deskmon_config_t config;

    deskmon_config_set_defaults(&config);

    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_OK);
    assert(config.carousel_interval_sec == DESKMON_CONFIG_DEFAULT_CAROUSEL_SEC);
    assert(config.sensor_read_interval_sec == DESKMON_CONFIG_DEFAULT_SENSOR_READ_SEC);
    assert(config.sensor_history_retention_hours == DESKMON_CONFIG_DEFAULT_SENSOR_HISTORY_HOURS);
    assert(config.enabled_pages.summary);
    assert(config.enabled_pages.weather);
    assert(config.enabled_pages.sensor);
    assert(config.enabled_pages.memo);
    assert(config.enabled_pages.album);
}

static void test_rejects_invalid_wifi_and_carousel_values(void)
{
    deskmon_config_t config;

    deskmon_config_set_defaults(&config);
    memset(config.wifi_ssid, 'A', sizeof(config.wifi_ssid));
    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_ERR_WIFI_SSID);

    deskmon_config_set_defaults(&config);
    strcpy(config.wifi_password, "short");
    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_ERR_WIFI_PASSWORD);

    deskmon_config_set_defaults(&config);
    config.carousel_interval_sec = 0;
    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_ERR_CAROUSEL_INTERVAL);

    deskmon_config_set_defaults(&config);
    config.sensor_read_interval_sec = 0;
    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_ERR_SENSOR_READ_INTERVAL);

    deskmon_config_set_defaults(&config);
    config.sensor_history_retention_hours = 0;
    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_ERR_SENSOR_HISTORY_RETENTION);
}

static void test_accepts_qweather_and_page_text(void)
{
    deskmon_config_t config;

    deskmon_config_set_defaults(&config);
    strcpy(config.qweather_api_key, "demo-key");
    strcpy(config.qweather_location, "101010100");
    strcpy(config.page_summary_note, "Desk Monitor / 桌面监视器");

    assert(deskmon_config_validate(&config) == DESKMON_CONFIG_OK);
}

int main(void)
{
    test_defaults_are_safe_for_first_boot();
    test_rejects_invalid_wifi_and_carousel_values();
    test_accepts_qweather_and_page_text();
    return 0;
}
