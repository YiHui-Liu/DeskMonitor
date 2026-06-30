#pragma once

#include <esp_err.h>

#define DESKMON_STORAGE_BASE_PATH "/littlefs"
#define DESKMON_CONFIG_FILE_PATH DESKMON_STORAGE_BASE_PATH "/config.json"

esp_err_t deskmon_storage_mount(void);
