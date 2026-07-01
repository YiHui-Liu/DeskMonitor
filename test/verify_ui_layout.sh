#!/usr/bin/env bash
set -euo pipefail

ui_pages="components/ui/src/ui_pages.c"
ui_display="components/ui/src/ui_display.c"
ui_pages_h="components/ui/include/ui/ui_pages.h"
ui_display_h="components/ui/include/ui/ui_display.h"
main_c="src/main.c"
app_cmake="components/app/CMakeLists.txt"
app_display_data="components/app/src/app_display_data.c"
controller_source="scripts/controller.html"
controller="data/controller.html"

require_pattern() {
  local pattern="$1"
  local file="$2"
  local message="$3"
  if ! rg -q "$pattern" "$file"; then
    printf '%s\n' "$message" >&2
    exit 1
  fi
}

require_pattern 'header_clock_update' "$ui_display" 'header clock must include live date and seconds'
require_pattern '2026-06-30 10:30:00' "$ui_display" 'header fallback must include date and seconds'
require_pattern 'weather_day_card' "$ui_pages" 'weather forecast must render individual day cards'
require_pattern '7月4' "$ui_pages" '5-day forecast must include five visible days'
require_pattern 'sensor_summary_card' "$ui_pages" 'home sensor area must use card-style sensor tiles'
require_pattern 'draw_chart_axes' "$ui_pages" 'sensor curves must draw x/y axes'
require_pattern 'draw_calendar_icon' "$ui_pages" 'calendar icon must be drawn as LVGL vector primitives'
require_pattern 'draw_weather_icon' "$ui_pages" 'weather icon must be drawn as LVGL vector primitives'
require_pattern 'draw_humidity_icon' "$ui_pages" 'humidity icon must be drawn as LVGL vector primitives'
require_pattern 'draw_wind_icon' "$ui_pages" 'wind icon must be drawn as LVGL vector primitives'
require_pattern 'draw_temperature_icon' "$ui_pages" 'sensor page temperature icon must be LVGL vector primitives'
require_pattern 'draw_light_icon' "$ui_pages" 'sensor page light icon must be LVGL vector primitives'
require_pattern 'draw_co2_icon' "$ui_pages" 'sensor page CO2 icon must be LVGL vector primitives'
require_pattern 'draw_tvoc_icon' "$ui_pages" 'sensor page TVOC icon must be LVGL vector primitives'
require_pattern 'sensor_pad_icon_t' "$ui_pages" 'sensor pads must select vector icons instead of text glyph icons'
require_pattern '最大 29 \| 最小 24 \| 平均 27' "$ui_pages" 'sensor stats must use compact 最大/最小/平均 format'
require_pattern 'sensor->status, 124, 1' "$ui_pages" 'sensor status flag must share the top row instead of a separate line'
require_pattern 'hourly_weather_row' "$ui_pages" 'weather hourly forecast must use two rows without a title label'
require_pattern '10:00.*11:00' "$ui_pages" 'weather hourly time row must show forecast times'
require_pattern '晴 30' "$ui_pages" 'weather hourly condition row must show weather plus temperature'
require_pattern 'deskmon_display_snapshot_t' "$ui_pages_h" 'UI pages must accept a live display snapshot'
require_pattern 'deskmon_display_snapshot_provider_t' "$ui_display_h" 'display config must support a live snapshot provider callback'
require_pattern 'deskmon_display_data_start' "$main_c" 'main must start the live display data updater'
require_pattern 'app_display_data.c' "$app_cmake" 'app component must compile live display data bridge'
require_pattern 'weather_refresh_interval_min' "$controller_source" 'web UI source must expose weather refresh interval config'
require_pattern 'weather_refresh_interval_min' "$controller" 'generated web UI must expose weather refresh interval config'
require_pattern 'weather_refresh_ticks' "$app_display_data" 'weather API refresh must use a cache interval gate'
require_pattern 'last_weather_attempt' "$app_display_data" 'weather API calls must be throttled between sensor loops'

if rg -n 'card_create\([^,]+, [0-9]+, 2[4-9][0-9],' "$ui_pages"; then
  printf 'page content must not place cards below the 236px content area\n' >&2
  exit 1
fi

printf 'ui layout checks verified\n'
