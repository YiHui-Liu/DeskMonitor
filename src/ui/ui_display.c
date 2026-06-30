#include "ui/ui_display.h"

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "deskmon_display";

esp_err_t deskmon_display_init(void)
{
    ESP_LOGW(TAG, "display is reserved until SPI pins are assigned");
    return ESP_ERR_NOT_SUPPORTED;
}

bool deskmon_display_is_enabled(void)
{
    return false;
}
