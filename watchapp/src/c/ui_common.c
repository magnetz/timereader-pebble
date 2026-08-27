#include "ui_common.h"
#include "seed.h"
#include "store.h"
#include "ui_list.h"
#include "ui_detail.h"
#include "ui_digit.h"
#include "ui_timer.h"
#include "ui_summary.h"
#include "ui_endmenu.h"

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

void ui_format_duration(char *buf, size_t n, int seconds) {
  session_format_duration(buf, (int)n, seconds);
}

/* --- window-stack routing --------------------------------------------- */

/* Desired window stack, deepest first. Windows not yet implemented in the
   current task resolve to NULL and are skipped. */
#define MAX_STACK 5
static Window *s_stack[MAX_STACK];
static int s_depth;

static int desired_stack(Window **out) {
  int d = 0;
  switch (g_ctx.state) {
    case APP_NO_BOOKS:
      out[d++] = ui_nobooks_window();
      break;
    case APP_LIST_BOOKS:
      out[d++] = ui_list_window();
      break;
    case APP_BOOK_DETAIL:
      out[d++] = ui_list_window();
      out[d++] = ui_detail_window();
      break;
    case APP_ENTER_START_PAGE:
      out[d++] = ui_list_window();
      out[d++] = ui_detail_window();
      out[d++] = ui_digit_window();
      break;
    case APP_RUNNING:
    case APP_PAUSED:
      out[d++] = ui_list_window();
      out[d++] = ui_detail_window();
      out[d++] = ui_timer_window();
      break;
    case APP_END_SESSION_MENU:
      out[d++] = ui_list_window();
      out[d++] = ui_detail_window();
      out[d++] = ui_timer_window();
      if (g_ctx.end_menu_confirming) {
        out[d++] = ui_endmenu_confirm_window();
      }
      break;
    case APP_ENTER_END_PAGE:
      out[d++] = ui_list_window();
      out[d++] = ui_detail_window();
      out[d++] = ui_timer_window();
      out[d++] = ui_digit_window();
      break;
    case APP_SESSION_SUMMARY:
      out[d++] = ui_list_window();
      out[d++] = ui_detail_window();
      out[d++] = ui_summary_window();
      break;
  }
  /* Compact out any NULLs (windows from later tasks). */
  int w = 0;
  for (int i = 0; i < d; i++) {
    if (out[i]) out[w++] = out[i];
  }
  return w;
}

void ui_route_to_state(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "route -> state %d (book %d)",
          (int)g_ctx.state, g_ctx.book_index);

  Window *want[MAX_STACK];
  int wantn = desired_stack(want);

  int k = 0;
  while (k < s_depth && k < wantn && s_stack[k] == want[k]) k++;

  /* Pop everything above the common prefix. */
  for (int i = s_depth - 1; i >= k; i--) {
    window_stack_remove(s_stack[i], false);
  }
  s_depth = k;

  /* Push the new suffix, animating only the final step. */
  for (int i = k; i < wantn; i++) {
    bool animated = (i == wantn - 1);
    window_stack_push(want[i], animated);
    s_stack[s_depth++] = want[i];
  }

  ui_endmenu_sync();
}

void ui_refresh_current(void) {
  switch (g_ctx.state) {
    case APP_NO_BOOKS:
    case APP_LIST_BOOKS:
      ui_list_refresh();
      break;
    case APP_BOOK_DETAIL:
      ui_detail_refresh();
      break;
    case APP_ENTER_START_PAGE:
    case APP_ENTER_END_PAGE:
      ui_digit_refresh();
      break;
    case APP_RUNNING:
    case APP_PAUSED:
    case APP_END_SESSION_MENU:
      ui_timer_refresh();
      break;
    case APP_SESSION_SUMMARY:
      ui_summary_refresh();
      break;
  }
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
