#include "app/app_gzip.h"

#include "esp_log.h"
#include "miniz.h"

#define GZIP_FHCRC    0x02
#define GZIP_FEXTRA   0x04
#define GZIP_FNAME    0x08
#define GZIP_FCOMMENT 0x10

static const char *TAG = "deskmon_gzip";

esp_err_t deskmon_gunzip(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, size_t *out_len) {
  if (src == NULL || dst == NULL || out_len == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (src_len < 18) {
    return ESP_ERR_INVALID_SIZE;
  }
  if (src[0] != 0x1f || src[1] != 0x8b || src[2] != 0x08) {
    return ESP_ERR_INVALID_VERSION;
  }

  const uint8_t flg = src[3];
  size_t off = 10;

  if (flg & GZIP_FEXTRA) {
    if (off + 2 > src_len) {
      return ESP_ERR_INVALID_SIZE;
    }
    const size_t xlen = (size_t)src[off] | ((size_t)src[off + 1] << 8);
    off += 2 + xlen;
    if (off > src_len) {
      return ESP_ERR_INVALID_SIZE;
    }
  }
  if (flg & GZIP_FNAME) {
    while (off < src_len && src[off] != 0) {
      ++off;
    }
    if (off >= src_len) {
      return ESP_ERR_INVALID_SIZE;
    }
    ++off;
  }
  if (flg & GZIP_FCOMMENT) {
    while (off < src_len && src[off] != 0) {
      ++off;
    }
    if (off >= src_len) {
      return ESP_ERR_INVALID_SIZE;
    }
    ++off;
  }
  if (flg & GZIP_FHCRC) {
    off += 2;
    if (off > src_len) {
      return ESP_ERR_INVALID_SIZE;
    }
  }
  if (off + 8 >= src_len) {
    return ESP_ERR_INVALID_SIZE;
  }

  const uint8_t *deflate_src = src + off;
  const size_t deflate_len = src_len - off - 8;

  ESP_LOGI(TAG, "flg=0x%02x off=%u deflate=%u dst_cap=%u", flg, (unsigned)off, (unsigned)deflate_len,
           (unsigned)dst_cap);

  size_t result = tinfl_decompress_mem_to_mem(dst, dst_cap, deflate_src, deflate_len,
                                               TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (result == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
    ESP_LOGW(TAG, "tinfl_decompress_mem_to_mem failed (deflate=%u)", (unsigned)deflate_len);
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "decompressed %u bytes", (unsigned)result);
  *out_len = result;
  return ESP_OK;
}
