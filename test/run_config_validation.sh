#!/usr/bin/env bash
set -euo pipefail

cc -std=c11 -Wall -Wextra -DUNIT_TEST -Icomponents/app/include -Itest \
  "test/config_validation_test.c" \
  "components/app/src/app_config_validate.c" \
  -o /tmp/deskmon_config_validation_test

/tmp/deskmon_config_validation_test
