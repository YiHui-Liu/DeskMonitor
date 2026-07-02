#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* Decompresses a gzip-compressed buffer into dst.
 *
 * Returns:
 *   ESP_OK                - success, *out_len holds decompressed size
 *   ESP_ERR_INVALID_VERSION - src is not gzip (caller may treat body as-is)
 *   ESP_ERR_INVALID_ARG  - NULL pointers
 *   ESP_ERR_INVALID_SIZE - truncated/corrupt gzip header
 *   ESP_FAIL             - inflate failure or output too small
 */
esp_err_t deskmon_gunzip(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, size_t *out_len);
