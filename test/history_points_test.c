#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "ui/ui_history_points.h"

static void test_maps_history_oldest_to_newest(void) {
  const float values[] = {20.0f, 25.0f, 30.0f};
  deskmon_history_point_t points[3];

  assert(deskmon_history_points(values, 3, 140, 40, points) == 3);
  assert(points[0].x == 0 && points[0].y == 40);
  assert(points[1].x == 70 && points[1].y == 20);
  assert(points[2].x == 140 && points[2].y == 0);
}

static void test_constant_history_stays_midline(void) {
  const float values[] = {42.0f, 42.0f, 42.0f, 42.0f};
  deskmon_history_point_t points[4];

  assert(deskmon_history_points(values, 4, 90, 30, points) == 4);
  assert(points[0].x == 0 && points[0].y == 15);
  assert(points[1].x == 30 && points[1].y == 15);
  assert(points[2].x == 60 && points[2].y == 15);
  assert(points[3].x == 90 && points[3].y == 15);
}

static void test_empty_history_returns_no_points(void) {
  deskmon_history_point_t points[1] = {{.x = 7, .y = 9}};

  assert(deskmon_history_points(NULL, 0, 140, 40, points) == 0);
  assert(points[0].x == 7 && points[0].y == 9);
}

int main(void) {
  test_maps_history_oldest_to_newest();
  test_constant_history_stays_midline();
  test_empty_history_returns_no_points();
  return 0;
}
