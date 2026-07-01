#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/ui_carousel.h"

typedef struct {
  bool enabled_pages[DESKMON_PAGE_COUNT];
  bool pending_enabled_pages[DESKMON_PAGE_COUNT];
  uint32_t carousel_period_ms;
  uint32_t pending_carousel_period_ms;
  bool pending;
} deskmon_display_config_state_t;

void deskmon_display_config_state_init(deskmon_display_config_state_t *state,
                                       const bool enabled_pages[DESKMON_PAGE_COUNT], uint32_t carousel_period_ms);
void deskmon_display_config_state_set_pending(deskmon_display_config_state_t *state,
                                              const bool enabled_pages[DESKMON_PAGE_COUNT],
                                              uint32_t carousel_period_ms);
bool deskmon_display_config_state_consume(deskmon_display_config_state_t *state, bool *changed);
