#include <pebble.h>
#include "ui_common.h"
#include "state_machine.h"
#include "seed.h"
#include "store.h"
#include "sync.h"

static void rebuild_model(void) {
  int n;
  const DigestBook *b = seed_books(&n);
  PersistedSession p = store_load_session();
  int now = (int)time(NULL);
  if (p.present) {
    sm_restore(&g_ctx, b, n, p.state, p.start_page, p.elapsed_seconds, now);
  } else {
    sm_init(&g_ctx, b, n, now);
  }
}

/* A fresh snapshot committed: re-read the cache and redraw. */
static void on_books_changed(void) {
  seed_reload();
  rebuild_model();
  ui_route_to_state();
  ui_refresh_current();
}

static void tick_handler(struct tm *t, TimeUnits u) {
  (void)t;
  ui_dispatch(EV_TICK);
  if (u & MINUTE_UNIT) sync_drain();   /* retry the queue every minute while open */
}

static void init(void) {
  sync_init(on_books_changed);
  rebuild_model();
  ui_route_to_state();
  tick_timer_service_subscribe(SECOND_UNIT | MINUTE_UNIT, tick_handler);
  sync_drain();
}

static void deinit(void) {
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
