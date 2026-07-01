#!/usr/bin/env bash
set -euo pipefail

cc -std=c11 -Wall -Wextra -DUNIT_TEST -Icomponents/ui/include -Itest \
  "test/history_points_test.c" \
  "components/ui/src/ui_history_points.c" \
  -o /tmp/deskmon_history_points_test

/tmp/deskmon_history_points_test
