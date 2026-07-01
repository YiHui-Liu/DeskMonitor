#pragma once

#include <stdbool.h>

#include <esp_err.h>

esp_err_t deskmon_buttons_init(void);
bool deskmon_buttons_are_enabled(void);
