#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "ui/ui_display_state.h"

static void test_pending_config_applies_enabled_pages_and_period(void) {
  deskmon_display_config_state_t state = {0};
  const bool initial[] = {true, true, false, true, false};
  const bool pending[] = {false, true, true, false, false};

  deskmon_display_config_state_init(&state, initial, 15000);
  deskmon_display_config_state_set_pending(&state, pending, 5000);

  bool changed = false;
  assert(deskmon_display_config_state_consume(&state, &changed));
  assert(changed);
  assert(state.carousel_period_ms == 5000);
  assert(!state.enabled_pages[0]);
  assert(state.enabled_pages[1]);
  assert(state.enabled_pages[2]);
  assert(!state.enabled_pages[3]);
  assert(!state.enabled_pages[4]);
}

static void test_consume_without_pending_reports_no_change(void) {
  deskmon_display_config_state_t state = {0};
  bool changed = true;

  deskmon_display_config_state_init(&state, NULL, 15000);
  assert(deskmon_display_config_state_consume(&state, &changed));
  assert(!changed);
  assert(state.carousel_period_ms == 15000);
}

int main(void) {
  test_pending_config_applies_enabled_pages_and_period();
  test_consume_without_pending_reports_no_change();
  return 0;
}
