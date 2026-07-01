#!/usr/bin/env bash
set -euo pipefail

cc -std=c11 -Wall -Wextra -DUNIT_TEST -Icomponents/ui/include -Itest \
  "test/display_config_state_test.c" \
  "components/ui/src/ui_display_state.c" \
  -o /tmp/deskmon_display_config_state_test

/tmp/deskmon_display_config_state_test
