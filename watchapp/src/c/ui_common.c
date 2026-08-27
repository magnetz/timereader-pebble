#include "ui_common.h"
#include "seed.h"
#include "store.h"
#include "ui_list.h"
#include "ui_detail.h"
#include "ui_digit.h"
#include "ui_timer.h"
#include "ui_summary.h"
#include "ui_endmenu.h"
#include "ui_digit.h"
#include "sync.h"
#include "digit_entry.h"
#include <string.h>

SmContext g_ctx;

/* id of the session most recently enqueued — the only one the Summary
   screen can retract. */
static char s_last_saved_session_id[16];

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
  static int s_logged = -1;
  if ((int)g_ctx.state != s_logged) {
    APP_LOG(APP_LOG_LEVEL_INFO, "route -> state %d (book %d)",
            (int)g_ctx.state, g_ctx.book_index);
    s_logged = (int)g_ctx.state;
  }

  Window *want[MAX_STACK];
  int wantn = desired_stack(want);

  int k = 0;
  while (k < s_depth && k < wantn && s_stack[k] == want[k]) k++;

  /* Pure back-navigation (only popping): let the system animate the top
     window sliding out, like every stock app. */
  bool pure_pop = (wantn == k) && (s_depth > k);
  for (int i = s_depth - 1; i >= k; i--) {
    bool animate = pure_pop && (i == s_depth - 1);
    if (animate) {
      window_stack_pop(true);
    } else {
      window_stack_remove(s_stack[i], false);
    }
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
    case FX_SAVE_SESSION: {
      QueuedSession qs;
      memset(&qs, 0, sizeof(qs));
      snprintf(qs.id, sizeof(qs.id), "w%d", store_next_session_seq());
      if (g_ctx.book_index >= 0 && g_ctx.book_index < n) {
        strncpy(qs.book_id, b[g_ctx.book_index].id, sizeof(qs.book_id) - 1);
      }
      qs.start_page = g_ctx.start_page_for_session;
      qs.end_page = digit_entry_value(&g_ctx.entry);
      qs.duration_seconds = g_ctx.last_session_seconds;
      strncpy(s_last_saved_session_id, qs.id, sizeof(s_last_saved_session_id) - 1);
      sync_enqueue_session(&qs);
      store_clear_session();
      break;
    }
    case FX_RETRACT_SESSION:
      sync_retract_session(s_last_saved_session_id);
      store_clear_session();
      break;
    case FX_DISCARD_SESSION:
      store_clear_session();
      break;
    case FX_PAGE_ERROR:
      ui_digit_flash_error();
      break;
    case FX_NONE:
      break;
  }

  /* Re-persist elapsed at most every 10s while running, so a crash loses
     no more than 10s (matches the original app's cadence). */
  if (ev == EV_TICK && g_ctx.state == APP_RUNNING) {
    static int s_last_persist;
    if (now - s_last_persist >= 10) {
      s_last_persist = now;
      store_save_session(APP_RUNNING, g_ctx.start_page_for_session,
                         live_session_elapsed(&g_ctx.live, now));
    }
  }

  ui_route_to_state();
  ui_refresh_current();
}
