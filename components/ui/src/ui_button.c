#include "ui/ui_button.h"

#include <stdbool.h>

#include <esp_err.h>
#include <esp_log.h>

static const char *TAG = "deskmon_button";

esp_err_t deskmon_buttons_init(void) {
  ESP_LOGW(TAG, "buttons are reserved and not enabled in this build");
  return ESP_ERR_NOT_SUPPORTED;
}

bool deskmon_buttons_are_enabled(void) { return false; }
