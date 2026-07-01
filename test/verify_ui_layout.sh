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
require_pattern 'UP Time: %d s' "$ui_display" 'header fallback must show uptime until SNTP sets date/time'
require_pattern 'weather_day_card' "$ui_pages" 'weather forecast must render individual day cards'
require_pattern 'DESKMON_DISPLAY_DAILY_COUNT' "$ui_pages" '5-day forecast must render from the live daily snapshot array'
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
require_pattern 'deskmon_history_points' "$ui_pages" 'sensor curves must use live snapshot history points'
require_pattern 'sensor->samples' "$ui_pages" 'sensor curves must render from snapshot sample arrays'
require_pattern 'sensor->status, 110, 1' "$ui_pages" 'sensor status flag must share the top row with the live reading'
require_pattern 'hourly_weather_row' "$ui_pages" 'weather hourly forecast must use two rows without a title label'
require_pattern 'snapshot->hourly\[i\].time' "$ui_pages" 'weather hourly time row must render from the live snapshot'
require_pattern 'snapshot->hourly\[i\].weather' "$ui_pages" 'weather hourly condition row must render from the live snapshot'
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
