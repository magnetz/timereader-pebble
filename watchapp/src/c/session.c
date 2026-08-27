#include "session.h"
#include <stdio.h>

void session_format_duration(char *buf, int buf_size, int seconds) {
  if (seconds < 0) seconds = 0;
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  if (h > 0) {
    snprintf(buf, buf_size, "%d:%02d:%02d", h, m, s);
  } else {
    snprintf(buf, buf_size, "%02d:%02d", m, s);
  }
}

void live_session_start(LiveSession *s, int start_page, int now) {
  s->start_page = start_page;
  s->accumulated_seconds = 0;
  s->segment_start = now;
}

void live_session_pause(LiveSession *s, int now) {
  if (s->segment_start >= 0) {
    s->accumulated_seconds += now - s->segment_start;
    s->segment_start = -1;
  }
}

void live_session_resume(LiveSession *s, int now) {
  if (s->segment_start < 0) s->segment_start = now;
}

int live_session_elapsed(const LiveSession *s, int now) {
  int e = s->accumulated_seconds;
  if (s->segment_start >= 0) e += now - s->segment_start;
  return e;
}

int total_hours_x100(const Session *ss, int n) {
  long secs = 0;
  for (int i = 0; i < n; i++) secs += ss[i].duration_seconds;
  return (int)((secs * 100) / 3600);
}

int pages_per_hour_x100(const Session *ss, int n) {
  long pages = 0, secs = 0;
  for (int i = 0; i < n; i++) {
    pages += ss[i].pages;
    secs += ss[i].duration_seconds;
  }
  if (secs == 0) return 0;
  return (int)((pages * 3600 * 100) / secs);
}

int current_page_from_sessions(const Session *ss, int n, int book_current_page) {
  return n > 0 ? ss[n - 1].end_page : book_current_page;
}

int pages_left(int total_pages, int current_page) {
  int d = total_pages - current_page;
  return d > 0 ? d : 0;
}

int eta_minutes(int pages_left_count, int pph_x100) {
  if (pph_x100 == 0) return 0;
  return (int)(((long)pages_left_count * 60 * 100) / pph_x100);
}
