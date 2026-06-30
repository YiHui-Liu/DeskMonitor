#!/usr/bin/env bash
set -euo pipefail

cc -std=c11 -Wall -Wextra -DUNIT_TEST -Icomponents/ui/include -Itest \
  "test/ui_carousel_test.c" \
  "components/ui/src/ui_carousel.c" \
  -o /tmp/deskmon_ui_carousel_test

/tmp/deskmon_ui_carousel_test
