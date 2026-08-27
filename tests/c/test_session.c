#include "session.h"
#include "test.h"

void t_elapsed_across_pause(void) {
  LiveSession s; live_session_start(&s, 10, 100);
  CHECK_EQ_INT(live_session_elapsed(&s, 130), 30, "");
  live_session_pause(&s, 130);
  CHECK_EQ_INT(live_session_elapsed(&s, 200), 30, "frozen while paused");
  live_session_resume(&s, 200);
  CHECK_EQ_INT(live_session_elapsed(&s, 210), 40, "");
}

void t_pph_is_sum_over_sum_not_mean_of_means(void) {
  /* A: 90 pages / 1h, B: 10 pages / 3h.  Sum/sum = 100 / 4h = 25.00 pph.
     Mean of per-session rates would be (90 + 3.33) / 2 = 46.67. Must be 2500. */
  Session ss[2] = {
    {.pages = 90, .duration_seconds = 3600},
    {.pages = 10, .duration_seconds = 10800},
  };
  CHECK_EQ_INT(pages_per_hour_x100(ss, 2), 2500, "");
  CHECK_EQ_INT(pages_per_hour_x100(ss, 0), 0, "no sessions -> 0");
}

void t_total_hours(void) {
  Session ss[2] = {
    {.duration_seconds = 3600},
    {.duration_seconds = 1800},
  };
  CHECK_EQ_INT(total_hours_x100(ss, 2), 150, "1.5h -> 150");
}

void t_current_page_prefers_last_session(void) {
  Session ss[2] = {
    {.end_page = 40, .duration_seconds = 60},
    {.end_page = 73, .duration_seconds = 60},
  };
  CHECK_EQ_INT(current_page_from_sessions(ss, 2, 5), 73, "");
  CHECK_EQ_INT(current_page_from_sessions(ss, 0, 5), 5, "falls back to book");
}

void t_pages_left_and_eta(void) {
  CHECK_EQ_INT(pages_left(300, 280), 20, "");
  CHECK_EQ_INT(pages_left(300, 320), 0, "clamped");
  CHECK_EQ_INT(eta_minutes(20, 4000), 30, "20 pages at 40 pph = 0.5h");
  CHECK_EQ_INT(eta_minutes(20, 0), 0, "no rate");
}

void t_format_duration(void) {
  char b[12];
  session_format_duration(b, sizeof(b), 0);      CHECK_EQ_STR(b, "00:00", "");
  session_format_duration(b, sizeof(b), 65);     CHECK_EQ_STR(b, "01:05", "");
  session_format_duration(b, sizeof(b), 3599);   CHECK_EQ_STR(b, "59:59", "just under an hour");
  session_format_duration(b, sizeof(b), 3600);   CHECK_EQ_STR(b, "1:00:00", "an hour");
  session_format_duration(b, sizeof(b), 4520);   CHECK_EQ_STR(b, "1:15:20", "over an hour");
  session_format_duration(b, sizeof(b), 36000);  CHECK_EQ_STR(b, "10:00:00", "ten hours");
  session_format_duration(b, sizeof(b), -5);     CHECK_EQ_STR(b, "00:00", "negative clamps");
}

TEST_BEGIN()
  t_elapsed_across_pause();
  t_pph_is_sum_over_sum_not_mean_of_means();
  t_total_hours();
  t_current_page_prefers_last_session();
  t_pages_left_and_eta();
  t_format_duration();
TEST_END()
