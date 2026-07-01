#pragma once

#include "ui/ui_carousel.h"

#include <stdbool.h>

#include "lvgl.h"

#define DESKMON_DISPLAY_SENSOR_COUNT 6
#define DESKMON_DISPLAY_HOURLY_COUNT 6
#define DESKMON_DISPLAY_DAILY_COUNT 5

typedef enum {
  DESKMON_SENSOR_ICON_TEMPERATURE = 0,
  DESKMON_SENSOR_ICON_HUMIDITY,
  DESKMON_SENSOR_ICON_LIGHT,
  DESKMON_SENSOR_ICON_CO2,
  DESKMON_SENSOR_ICON_TVOC,
  DESKMON_SENSOR_ICON_AQI,
} deskmon_sensor_icon_t;

typedef enum {
  DESKMON_WEATHER_ICON_SUN = 0,
  DESKMON_WEATHER_ICON_CLOUD,
  DESKMON_WEATHER_ICON_RAIN,
} deskmon_weather_icon_t;

typedef struct {
  const char *name;
  deskmon_sensor_icon_t icon;
  char reading[16];
  char status[8];
  char stats[40];
  bool valid;
} deskmon_display_sensor_t;

typedef struct {
  char time[8];
  char weather[16];
} deskmon_display_hourly_t;

typedef struct {
  char date[8];
  char temp[12];
  deskmon_weather_icon_t icon;
} deskmon_display_daily_t;

typedef struct {
  char date[16];
  char weekday[12];
  char time[8];
  char location[24];
  char weather_text[16];
  char temperature[12];
  char humidity[12];
  char wind[24];
  char high[12];
  char low[12];
  char pressure[16];
  char feels_like[12];
  char precip[12];
  deskmon_display_hourly_t hourly[DESKMON_DISPLAY_HOURLY_COUNT];
  deskmon_display_daily_t daily[DESKMON_DISPLAY_DAILY_COUNT];
  deskmon_display_sensor_t sensors[DESKMON_DISPLAY_SENSOR_COUNT];
  bool weather_valid;
  bool sensors_valid;
} deskmon_display_snapshot_t;

void deskmon_display_snapshot_defaults(deskmon_display_snapshot_t *snapshot);
lv_obj_t *deskmon_page_create(deskmon_page_id_t page, lv_obj_t *parent, const deskmon_display_snapshot_t *snapshot);
