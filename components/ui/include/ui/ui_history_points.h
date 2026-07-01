#pragma once

#include <stdint.h>

typedef struct {
  int32_t x;
  int32_t y;
} deskmon_history_point_t;

uint32_t deskmon_history_points(const float *values, uint32_t count, int32_t width, int32_t height,
                                deskmon_history_point_t *points);
