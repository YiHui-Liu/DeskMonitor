#include "storage/tf_album.h"

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "deskmon_album";

esp_err_t deskmon_tf_album_init(void)
{
    ESP_LOGW(TAG, "TF album storage is reserved until a card is installed");
    return ESP_ERR_NOT_SUPPORTED;
}

bool deskmon_tf_album_is_enabled(void)
{
    return false;
}
