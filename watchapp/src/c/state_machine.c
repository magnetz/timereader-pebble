#include "state_machine.h"

#include <string.h>

/* Where a new session should start reading: the page after the last one
   already read. A never-started book begins at page 1 (the user overrides
   it by hand anyway). */
static int prefill_start_page(const DigestBook *book) {
  return book->current_page > 0 ? book->current_page + 1 : 1;
}

static void reset_menu(SmContext *c) {
  c->end_menu_index = 0;
  c->end_menu_confirming = false;
}

void sm_init(SmContext *c, const DigestBook *books, int book_count, int now) {
  (void)books;
  (void)now;
  memset(c, 0, sizeof(*c));
  c->book_count = book_count;
  c->book_index = book_count > 0 ? 0 : -1;
  c->detail_page = 0;
  c->state = book_count > 0 ? APP_LIST_BOOKS : APP_NO_BOOKS;
  c->live.segment_start = -1;
}

void sm_restore(SmContext *c, const DigestBook *books, int book_count,
                AppState persisted_state, int persisted_start_page,
                int persisted_elapsed_seconds, int now) {
  sm_init(c, books, book_count, now);
  if (persisted_state == APP_RUNNING || persisted_state == APP_PAUSED ||
      persisted_state == APP_ENTER_END_PAGE) {
    c->state = APP_PAUSED; /* never auto-resume */
    c->start_page_for_session = persisted_start_page;
    c->live.start_page = persisted_start_page;
    c->live.accumulated_seconds = persisted_elapsed_seconds;
    c->live.segment_start = -1;
  }
}

/* Shared handler for ENTER_START_PAGE / ENTER_END_PAGE. */
static SideEffect handle_digit(SmContext *c, const DigestBook *books, Event ev,
                               int now, bool is_end) {
  (void)books;
  switch (ev) {
    case EV_UP:   digit_entry_up(&c->entry); return FX_NONE;
    case EV_DOWN: digit_entry_down(&c->entry); return FX_NONE;
    case EV_BACK:
    case EV_BACK_LONG:
      if (digit_entry_back(&c->entry)) {
        if (is_end) {
          c->state = c->state_before_end_page; /* RUNNING or PAUSED; live untouched */
        } else {
          c->state = APP_BOOK_DETAIL;
        }
      }
      return FX_NONE;
    case EV_SELECT:
      if (!digit_entry_select(&c->entry)) return FX_NONE;
      /* last digit confirmed */
      if (!is_end) {
        c->start_page_for_session = digit_entry_value(&c->entry);
        live_session_start(&c->live, c->start_page_for_session, now);
        c->state = APP_RUNNING;
        return FX_PERSIST_STATE;
      } else {
        int end = digit_entry_value(&c->entry);
        if (end < c->start_page_for_session) return FX_PAGE_ERROR;
        /* Inclusive: reading pages S..E covers (E - S + 1) pages. */
        c->last_session_pages = end - c->start_page_for_session + 1;
        c->last_session_seconds = live_session_elapsed(&c->live, now);
        c->state = APP_SESSION_SUMMARY;
        return FX_SAVE_SESSION;
      }
    case EV_TICK:
    default:
      return FX_NONE;
  }
}

static void open_end_menu(SmContext *c, AppState from) {
  c->state_before_end_page = from;
  c->state = APP_END_SESSION_MENU;
  reset_menu(c);
}

SideEffect sm_handle(SmContext *c, const DigestBook *books, Event ev, int now) {
  switch (c->state) {
    case APP_NO_BOOKS:
      return FX_NONE;

    case APP_LIST_BOOKS:
      if (c->book_count <= 0) return FX_NONE;
      if (ev == EV_DOWN) {
        c->book_index = (c->book_index + 1) % c->book_count;
      } else if (ev == EV_UP) {
        c->book_index = (c->book_index - 1 + c->book_count) % c->book_count;
      } else if (ev == EV_SELECT) {
        c->detail_page = 0;
        c->state = APP_BOOK_DETAIL;
      }
      return FX_NONE;

    case APP_BOOK_DETAIL:
      if (ev == EV_DOWN) {
        c->detail_page = (c->detail_page + 1) % 3;
      } else if (ev == EV_UP) {
        c->detail_page = (c->detail_page + 2) % 3;
      } else if (ev == EV_BACK || ev == EV_BACK_LONG) {
        c->state = APP_LIST_BOOKS;
      } else if (ev == EV_SELECT) {
        if (books[c->book_index].color != BOOK_COMPLETED) {
          digit_entry_init(&c->entry, prefill_start_page(&books[c->book_index]));
          c->state = APP_ENTER_START_PAGE;
        }
      }
      return FX_NONE;

    case APP_ENTER_START_PAGE:
      return handle_digit(c, books, ev, now, false);

    case APP_ENTER_END_PAGE:
      return handle_digit(c, books, ev, now, true);

    case APP_RUNNING:
      if (ev == EV_SELECT) {
        live_session_pause(&c->live, now);
        c->state = APP_PAUSED;
        return FX_PERSIST_STATE;
      } else if (ev == EV_BACK || ev == EV_BACK_LONG) {
        open_end_menu(c, APP_RUNNING);
      }
      return FX_NONE;

    case APP_PAUSED:
      if (ev == EV_SELECT) {
        live_session_resume(&c->live, now);
        c->state = APP_RUNNING;
        return FX_PERSIST_STATE;
      } else if (ev == EV_BACK || ev == EV_BACK_LONG) {
        open_end_menu(c, APP_PAUSED);
      }
      return FX_NONE;

    case APP_END_SESSION_MENU:
      if (c->end_menu_confirming) {
        if (ev == EV_SELECT) {
          c->state = APP_BOOK_DETAIL;
          reset_menu(c);
          return FX_DISCARD_SESSION;
        } else if (ev == EV_BACK || ev == EV_BACK_LONG) {
          c->end_menu_confirming = false;
        }
        return FX_NONE;
      }
      if (ev == EV_DOWN) {
        if (c->end_menu_index < 2) c->end_menu_index++;
      } else if (ev == EV_UP) {
        if (c->end_menu_index > 0) c->end_menu_index--;
      } else if (ev == EV_BACK || ev == EV_BACK_LONG) {
        c->state = c->state_before_end_page;
      } else if (ev == EV_SELECT) {
        if (c->end_menu_index == 0) {
          digit_entry_init(&c->entry, c->start_page_for_session);
          c->state = APP_ENTER_END_PAGE;
        } else if (c->end_menu_index == 1) {
          c->end_menu_confirming = true;
        } else {
          c->state = c->state_before_end_page;
        }
      }
      return FX_NONE;

    case APP_SESSION_SUMMARY:
      if (ev == EV_SELECT || ev == EV_BACK) {
        c->state = APP_BOOK_DETAIL;
        return FX_NONE;
      } else if (ev == EV_UP || ev == EV_BACK_LONG) {
        c->live.accumulated_seconds = c->last_session_seconds;
        c->live.segment_start = -1;
        c->state = APP_PAUSED;
        return FX_RETRACT_SESSION;
      }
      return FX_NONE;

    default:
      return FX_NONE;
  }
}
