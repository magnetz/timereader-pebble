#ifndef TR_SESSION_H
#define TR_SESSION_H

#include "model.h"

/* A running/paused reading session, timed from monotonic second counts. */
typedef struct {
  int start_page;
  int accumulated_seconds; /* banked before the current running segment */
  int segment_start;       /* monotonic seconds when the RUNNING segment began; -1 if paused */
} LiveSession;

void live_session_start(LiveSession *s, int start_page, int now);
void live_session_pause(LiveSession *s, int now);
void live_session_resume(LiveSession *s, int now);
int  live_session_elapsed(const LiveSession *s, int now);

/* Stats over a book's finished sessions. */
int pages_per_hour_x100(const Session *sessions, int count);        /* Sum pages / (Sum secs / 3600), *100; 0 if no time */
int total_hours_x100(const Session *sessions, int count);
int current_page_from_sessions(const Session *sessions, int count, int book_current_page);
int pages_left(int total_pages, int current_page);                  /* max(0, total - current) */
int eta_minutes(int pages_left_count, int pph_x100);                /* 0 if pph_x100 == 0 */

#endif
