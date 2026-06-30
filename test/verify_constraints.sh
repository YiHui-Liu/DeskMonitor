#!/usr/bin/env bash
set -euo pipefail

forbidden='esp_lcd_new_panel|esp_lcd_panel_io|esp_lcd_panel_init|gpio_config|gpio_set_direction|gpio_intr_enable|esp_vfs_fat_sd|sdspi_|sdmmc_host_init|sdmmc_card_init'

if rg -n -g '*.c' -g '*.h' "$forbidden" src; then
  printf 'disabled hardware init symbol found\n' >&2
  exit 1
fi

if rg -n -g '*.c' -g '*.h' 'lv_init\(|lv_timer_handler\(' src; then
  printf 'LVGL runtime init found before display enablement\n' >&2
  exit 1
fi

printf 'disabled hardware constraints verified\n'
