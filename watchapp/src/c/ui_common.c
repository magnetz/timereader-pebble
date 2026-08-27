#include "ui_common.h"
#include "seed.h"
#include "store.h"

SmContext g_ctx;

const DigestBook *ui_books(int *count) {
  return seed_books(count);
}

GColor ui_color_for_book_state(BookColorState s) {
  switch (s) {
    case BOOK_COMPLETED: return GColorIslamicGreen;
    case BOOK_STARTED:   return GColorCyan;
    case BOOK_UNREAD:    return GColorWhite;
  }
  return GColorWhite;
}

void ui_setup_status_bar(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  StatusBarLayer *sb = status_bar_layer_create();
  status_bar_layer_set_colors(sb, GColorBlack, GColorWhite);
  status_bar_layer_set_separator_mode(sb, StatusBarLayerSeparatorModeDotted);
  layer_set_frame(status_bar_layer_get_layer(sb),
                  GRect(0, 0, b.size.w, STATUS_BAR_LAYER_HEIGHT));
  layer_add_child(root, status_bar_layer_get_layer(sb));
}

/* --- routing / dispatch (stubs until the window tasks fill them in) --- */

void ui_route_to_state(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "route -> state %d (book %d)",
          (int)g_ctx.state, g_ctx.book_index);
}

void ui_refresh_current(void) {
}

void ui_dispatch(Event ev) {
  int now = (int)time(NULL);
  int n;
  const DigestBook *b = ui_books(&n);
  SideEffect fx = sm_handle(&g_ctx, b, ev, now);

  switch (fx) {
    case FX_PERSIST_STATE:
    case FX_START_SESSION:
      store_save_session(g_ctx.state, g_ctx.start_page_for_session,
                         live_session_elapsed(&g_ctx.live, now));
      break;
    case FX_SAVE_SESSION:
    case FX_DISCARD_SESSION:
    case FX_RETRACT_SESSION:
      store_clear_session();
      break;
    case FX_PAGE_ERROR:
    case FX_NONE:
      break;
  }

  ui_route_to_state();
  ui_refresh_current();
}
