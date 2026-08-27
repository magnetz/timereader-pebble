#ifndef TR_STATE_MACHINE_H
#define TR_STATE_MACHINE_H

#include "model.h"
#include "digit_entry.h"
#include "session.h"

typedef enum {
  FX_NONE,
  FX_START_SESSION,
  FX_SAVE_SESSION,
  FX_DISCARD_SESSION,
  FX_RETRACT_SESSION,
  FX_PAGE_ERROR,
  FX_PERSIST_STATE
} SideEffect;

typedef struct {
  AppState state;
  int book_index;   /* into the book array; -1 if none */
  int book_count;
  int detail_page;  /* 0..2 */
  int end_menu_index;        /* 0..2 within END_SESSION_MENU */
  bool end_menu_confirming;  /* second confirm for "exit without saving" */
  DigitEntry entry;
  LiveSession live;
  AppState state_before_end_page; /* RUNNING or PAUSED, restored on cancel */
  int start_page_for_session;
  int last_session_pages;    /* for the summary screen */
  int last_session_seconds;
} SmContext;

void sm_init(SmContext *c, const DigestBook *books, int book_count, int now);
void sm_restore(SmContext *c, const DigestBook *books, int book_count,
                AppState persisted_state, int persisted_start_page,
                int persisted_elapsed_seconds, int now);
SideEffect sm_handle(SmContext *c, const DigestBook *books, Event ev, int now);

#endif
