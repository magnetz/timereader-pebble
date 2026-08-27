#include <pebble.h>
#include "ui_common.h"
#include "state_machine.h"
#include "seed.h"
#include "store.h"

static void tick_handler(struct tm *t, TimeUnits u) {
  (void)t;
  (void)u;
  ui_dispatch(EV_TICK);
}

static void init(void) {
  int n;
  const DigestBook *b = seed_books(&n);
  PersistedSession p = store_load_session();
  int now = (int)time(NULL);
  if (p.present) {
    sm_restore(&g_ctx, b, n, p.state, p.start_page, p.elapsed_seconds, now);
  } else {
    sm_init(&g_ctx, b, n, now);
  }
  ui_route_to_state();
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
