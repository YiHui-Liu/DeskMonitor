#!/usr/bin/env bash
set -euo pipefail

# Display (SPI ST7796S + LVGL) is enabled. SD/TF card, touch, and button
# interrupt GPIO remain disabled at runtime.

sd_forbidden='esp_vfs_fat_sd|sdspi_|sdmmc_host_init|sdmmc_card_init|sdmmc_init|esp_lcd_panel_sd'
if rg -n -g '*.c' -g '*.h' "$sd_forbidden" src components; then
  printf 'SD/TF card init symbol found\n' >&2
  exit 1
fi

intr_forbidden='gpio_intr_enable|gpio_isr_handler_add|gpio_install_isr_service|gpio_evt_queue|xpt2046|esp_lcd_touch'
if rg -n -g '*.c' -g '*.h' "$intr_forbidden" src components; then
  printf 'button/touch interrupt GPIO init found\n' >&2
  exit 1
fi

if rg -n -g '*.h' 'define[[:space:]]+DESKMON_BUTTONS_ENABLED[[:space:]]+[1-9]' src components; then
  printf 'buttons enabled before wiring is final\n' >&2
  exit 1
fi

printf 'hardware constraints verified (display enabled; SD/buttons/touch off)\n'
