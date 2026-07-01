#include "ui/ui_history_points.h"

#include <stddef.h>

uint32_t deskmon_history_points(const float *values, uint32_t count, int32_t width, int32_t height,
                                deskmon_history_point_t *points) {
  if (values == NULL || points == NULL || count == 0 || width < 0 || height < 0) {
    return 0;
  }

  float min = values[0];
  float max = values[0];
  for (uint32_t i = 1; i < count; ++i) {
    min = values[i] < min ? values[i] : min;
    max = values[i] > max ? values[i] : max;
  }

  const float span = max - min;
  for (uint32_t i = 0; i < count; ++i) {
    points[i].x = count > 1 ? (int32_t)((int64_t)width * (int64_t)i / (int64_t)(count - 1U)) : 0;
    if (span == 0.0f) {
      points[i].y = height / 2;
    } else {
      const float normalized = (values[i] - min) / span;
      points[i].y = height - (int32_t)(normalized * (float)height + 0.5f);
    }
  }

  return count;
}
