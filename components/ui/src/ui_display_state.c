#include "ui/ui_display_state.h"

#include <stddef.h>

static void copy_enabled_pages(bool dst[DESKMON_PAGE_COUNT], const bool src[DESKMON_PAGE_COUNT]) {
  for (int i = 0; i < DESKMON_PAGE_COUNT; ++i) {
    dst[i] = src != NULL ? src[i] : true;
  }
}

void deskmon_display_config_state_init(deskmon_display_config_state_t *state,
                                       const bool enabled_pages[DESKMON_PAGE_COUNT], uint32_t carousel_period_ms) {
  if (state == NULL) {
    return;
  }
  copy_enabled_pages(state->enabled_pages, enabled_pages);
  copy_enabled_pages(state->pending_enabled_pages, enabled_pages);
  state->carousel_period_ms = carousel_period_ms;
  state->pending_carousel_period_ms = carousel_period_ms;
  state->pending = false;
}

void deskmon_display_config_state_set_pending(deskmon_display_config_state_t *state,
                                              const bool enabled_pages[DESKMON_PAGE_COUNT],
                                              uint32_t carousel_period_ms) {
  if (state == NULL) {
    return;
  }
  copy_enabled_pages(state->pending_enabled_pages, enabled_pages);
  state->pending_carousel_period_ms = carousel_period_ms;
  state->pending = true;
}

bool deskmon_display_config_state_consume(deskmon_display_config_state_t *state, bool *changed) {
  if (state == NULL || changed == NULL) {
    return false;
  }
  *changed = false;
  if (!state->pending) {
    return true;
  }
  copy_enabled_pages(state->enabled_pages, state->pending_enabled_pages);
  state->carousel_period_ms = state->pending_carousel_period_ms;
  state->pending = false;
  *changed = true;
  return true;
}
