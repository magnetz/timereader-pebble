#include "state_machine.h"
#include "test.h"

static DigestBook BOOKS[2] = {
  {.id = "b1", .title = "Alpha", .current_page = 10, .total_pages = 200, .color = BOOK_STARTED},
  {.id = "b2", .title = "Beta",  .current_page = 0,  .total_pages = 0,   .color = BOOK_UNREAD},
};

/* Drive to APP_RUNNING with start page = book 0's next page (11). */
static void to_running(SmContext *c, int now) {
  sm_init(c, BOOKS, 2, 0);
  sm_handle(c, BOOKS, EV_SELECT, 0);   /* list -> detail */
  sm_handle(c, BOOKS, EV_SELECT, 0);   /* detail -> ENTER_START_PAGE (prefill 11) */
  for (int i = 0; i < 4; i++) sm_handle(c, BOOKS, EV_SELECT, now);
}

void t_list_navigation_wraps(void) {
  SmContext c; sm_init(&c, BOOKS, 2, 0);
  CHECK_EQ_INT(c.state, APP_LIST_BOOKS, "");
  sm_handle(&c, BOOKS, EV_DOWN, 0); CHECK_EQ_INT(c.book_index, 1, "");
  sm_handle(&c, BOOKS, EV_DOWN, 0); CHECK_EQ_INT(c.book_index, 0, "wrap");
  sm_handle(&c, BOOKS, EV_UP, 0);   CHECK_EQ_INT(c.book_index, 1, "wrap back");
}

void t_open_detail_and_back(void) {
  SmContext c; sm_init(&c, BOOKS, 2, 0);
  sm_handle(&c, BOOKS, EV_SELECT, 0); CHECK_EQ_INT(c.state, APP_BOOK_DETAIL, "");
  sm_handle(&c, BOOKS, EV_BACK, 0);   CHECK_EQ_INT(c.state, APP_LIST_BOOKS, "");
}

void t_detail_pages_cycle(void) {
  SmContext c; sm_init(&c, BOOKS, 2, 0);
  sm_handle(&c, BOOKS, EV_SELECT, 0);
  sm_handle(&c, BOOKS, EV_DOWN, 0); CHECK_EQ_INT(c.detail_page, 1, "");
  sm_handle(&c, BOOKS, EV_DOWN, 0); CHECK_EQ_INT(c.detail_page, 2, "");
  sm_handle(&c, BOOKS, EV_DOWN, 0); CHECK_EQ_INT(c.detail_page, 0, "wrap");
  sm_handle(&c, BOOKS, EV_UP, 0);   CHECK_EQ_INT(c.detail_page, 2, "wrap back");
}

void t_start_session_prefills_next_page(void) {
  SmContext c; sm_init(&c, BOOKS, 2, 0);
  sm_handle(&c, BOOKS, EV_SELECT, 0);
  SideEffect fx = sm_handle(&c, BOOKS, EV_SELECT, 0);
  CHECK_EQ_INT(c.state, APP_ENTER_START_PAGE, "");
  CHECK_EQ_INT(digit_entry_value(&c.entry), 11, "prefilled with current_page + 1");
  CHECK_EQ_INT(fx, FX_NONE, "");
}

void t_unread_book_prefills_page_one(void) {
  SmContext c; sm_init(&c, BOOKS, 2, 0);
  sm_handle(&c, BOOKS, EV_DOWN, 0);     /* select book 1 (current_page 0) */
  sm_handle(&c, BOOKS, EV_SELECT, 0);   /* detail */
  sm_handle(&c, BOOKS, EV_SELECT, 0);   /* ENTER_START_PAGE */
  CHECK_EQ_INT(digit_entry_value(&c.entry), 1, "unread book starts at page 1");
}

void t_completed_book_cannot_start(void) {
  DigestBook done[1] = {
    {.id = "c", .title = "Done", .current_page = 200, .total_pages = 200, .color = BOOK_COMPLETED},
  };
  SmContext c; sm_init(&c, done, 1, 0);
  sm_handle(&c, done, EV_SELECT, 0);
  sm_handle(&c, done, EV_SELECT, 0);
  CHECK_EQ_INT(c.state, APP_BOOK_DETAIL, "stays put, no session");
}

void t_confirm_start_page_enters_running(void) {
  SmContext c; to_running(&c, 100);
  CHECK_EQ_INT(c.state, APP_RUNNING, "");
  CHECK_EQ_INT(c.start_page_for_session, 11, "current_page 10 -> start 11");
  CHECK_EQ_INT(live_session_elapsed(&c.live, 130), 30, "");
}

void t_back_on_first_digit_of_start_cancels_to_detail(void) {
  SmContext c; sm_init(&c, BOOKS, 2, 0);
  sm_handle(&c, BOOKS, EV_SELECT, 0);
  sm_handle(&c, BOOKS, EV_SELECT, 0);
  sm_handle(&c, BOOKS, EV_BACK, 0);
  CHECK_EQ_INT(c.state, APP_BOOK_DETAIL, "");
}

void t_pause_resume(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_SELECT, 110); CHECK_EQ_INT(c.state, APP_PAUSED, "");
  CHECK_EQ_INT(live_session_elapsed(&c.live, 200), 10, "frozen at pause");
  sm_handle(&c, BOOKS, EV_SELECT, 210); CHECK_EQ_INT(c.state, APP_RUNNING, "");
  CHECK_EQ_INT(live_session_elapsed(&c.live, 220), 20, "");
}

void t_end_menu_cancel_returns_to_timer(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 110);
  CHECK_EQ_INT(c.state, APP_END_SESSION_MENU, "");
  sm_handle(&c, BOOKS, EV_DOWN, 110);
  sm_handle(&c, BOOKS, EV_DOWN, 110);   /* index -> 2 "Annulla" */
  sm_handle(&c, BOOKS, EV_SELECT, 110);
  CHECK_EQ_INT(c.state, APP_RUNNING, "cancel returns to timer");
}

void t_end_menu_save_opens_end_page_then_back_to_timer(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 120);
  sm_handle(&c, BOOKS, EV_SELECT, 120);  /* index 0 "Salva pagina finale" */
  CHECK_EQ_INT(c.state, APP_ENTER_END_PAGE, "");
  CHECK_EQ_INT(digit_entry_value(&c.entry), c.start_page_for_session, "prefilled with start page");
  sm_handle(&c, BOOKS, EV_BACK, 120);    /* back on first digit -> timer */
  CHECK_EQ_INT(c.state, APP_RUNNING, "");
  CHECK_EQ_INT(live_session_elapsed(&c.live, 130), 30, "elapsed untouched");
}

void t_end_menu_exit_without_saving_needs_second_confirm(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 130);
  sm_handle(&c, BOOKS, EV_DOWN, 130);    /* index 1 "Esci senza salvare" */
  sm_handle(&c, BOOKS, EV_SELECT, 130);
  CHECK(c.end_menu_confirming, "asks for a second confirm");
  CHECK_EQ_INT(c.state, APP_END_SESSION_MENU, "still on the menu state");
  SideEffect fx = sm_handle(&c, BOOKS, EV_SELECT, 130);
  CHECK_EQ_INT(fx, FX_DISCARD_SESSION, "");
  CHECK_EQ_INT(c.state, APP_BOOK_DETAIL, "");
}

void t_end_menu_confirm_back_aborts_confirm(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 130);
  sm_handle(&c, BOOKS, EV_DOWN, 130);
  sm_handle(&c, BOOKS, EV_SELECT, 130);
  CHECK(c.end_menu_confirming, "");
  sm_handle(&c, BOOKS, EV_BACK, 130);
  CHECK(!c.end_menu_confirming, "back cancels the confirm");
  CHECK_EQ_INT(c.state, APP_END_SESSION_MENU, "");
}

void t_end_page_below_start_errors(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 100);
  sm_handle(&c, BOOKS, EV_SELECT, 100);  /* ENTER_END_PAGE, prefill 11 */
  c.entry.digits[2] = 0; c.entry.digits[3] = 5;  /* 0005 < 11 */
  SideEffect fx = FX_NONE;
  for (int i = 0; i < 4; i++) fx = sm_handle(&c, BOOKS, EV_SELECT, 100);
  CHECK_EQ_INT(fx, FX_PAGE_ERROR, "");
  CHECK_EQ_INT(c.state, APP_ENTER_END_PAGE, "stays to correct");
}

void t_valid_end_page_saves_then_summary(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 700);
  sm_handle(&c, BOOKS, EV_SELECT, 700);  /* ENTER_END_PAGE */
  c.entry.digits[2] = 3; c.entry.digits[3] = 4;  /* 0034 */
  SideEffect fx = FX_NONE;
  for (int i = 0; i < 4; i++) fx = sm_handle(&c, BOOKS, EV_SELECT, 700);
  CHECK_EQ_INT(fx, FX_SAVE_SESSION, "");
  CHECK_EQ_INT(c.state, APP_SESSION_SUMMARY, "");
  CHECK_EQ_INT(c.last_session_pages, 24, "inclusive: 34 - 11 + 1");
  CHECK_EQ_INT(c.last_session_seconds, 600, "elapsed 100..700");
}

void t_summary_retract_returns_to_paused(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 700);
  sm_handle(&c, BOOKS, EV_SELECT, 700);
  c.entry.digits[2] = 3; c.entry.digits[3] = 4;
  for (int i = 0; i < 4; i++) sm_handle(&c, BOOKS, EV_SELECT, 700);
  SideEffect fx = sm_handle(&c, BOOKS, EV_UP, 700);
  CHECK_EQ_INT(fx, FX_RETRACT_SESSION, "");
  CHECK_EQ_INT(c.state, APP_PAUSED, "");
  CHECK_EQ_INT(live_session_elapsed(&c.live, 999), 600, "elapsed preserved, frozen");
}

void t_summary_long_back_also_retracts(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 700);
  sm_handle(&c, BOOKS, EV_SELECT, 700);
  c.entry.digits[2] = 3; c.entry.digits[3] = 4;
  for (int i = 0; i < 4; i++) sm_handle(&c, BOOKS, EV_SELECT, 700);
  SideEffect fx = sm_handle(&c, BOOKS, EV_BACK_LONG, 700);
  CHECK_EQ_INT(fx, FX_RETRACT_SESSION, "");
  CHECK_EQ_INT(c.state, APP_PAUSED, "");
}

void t_summary_confirm_returns_to_detail(void) {
  SmContext c; to_running(&c, 100);
  sm_handle(&c, BOOKS, EV_BACK, 700);
  sm_handle(&c, BOOKS, EV_SELECT, 700);
  c.entry.digits[2] = 3; c.entry.digits[3] = 4;
  for (int i = 0; i < 4; i++) sm_handle(&c, BOOKS, EV_SELECT, 700);
  sm_handle(&c, BOOKS, EV_SELECT, 700);
  CHECK_EQ_INT(c.state, APP_BOOK_DETAIL, "");
}

void t_restore_running_as_paused(void) {
  SmContext c;
  sm_restore(&c, BOOKS, 2, APP_RUNNING, 12, 420, 1000);
  CHECK_EQ_INT(c.state, APP_PAUSED, "never resume automatically");
  CHECK_EQ_INT(c.start_page_for_session, 12, "");
  CHECK_EQ_INT(live_session_elapsed(&c.live, 5000), 420, "frozen at persisted elapsed");
}

void t_no_books_state(void) {
  SmContext c; sm_init(&c, BOOKS, 0, 0);
  CHECK_EQ_INT(c.state, APP_NO_BOOKS, "");
  sm_handle(&c, BOOKS, EV_SELECT, 0);
  CHECK_EQ_INT(c.state, APP_NO_BOOKS, "select does nothing");
}

TEST_BEGIN()
  t_list_navigation_wraps();
  t_open_detail_and_back();
  t_detail_pages_cycle();
  t_start_session_prefills_next_page();
  t_unread_book_prefills_page_one();
  t_completed_book_cannot_start();
  t_confirm_start_page_enters_running();
  t_back_on_first_digit_of_start_cancels_to_detail();
  t_pause_resume();
  t_end_menu_cancel_returns_to_timer();
  t_end_menu_save_opens_end_page_then_back_to_timer();
  t_end_menu_exit_without_saving_needs_second_confirm();
  t_end_menu_confirm_back_aborts_confirm();
  t_end_page_below_start_errors();
  t_valid_end_page_saves_then_summary();
  t_summary_retract_returns_to_paused();
  t_summary_long_back_also_retracts();
  t_summary_confirm_returns_to_detail();
  t_restore_running_as_paused();
  t_no_books_state();
TEST_END()
