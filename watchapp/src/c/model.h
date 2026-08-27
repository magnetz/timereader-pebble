#ifndef TR_MODEL_H
#define TR_MODEL_H

#include <stdbool.h>

typedef enum {
  APP_LIST_BOOKS,
  APP_BOOK_DETAIL,
  APP_ENTER_START_PAGE,
  APP_RUNNING,
  APP_PAUSED,
  APP_END_SESSION_MENU,
  APP_ENTER_END_PAGE,
  APP_SESSION_SUMMARY,
  APP_NO_BOOKS
} AppState;

typedef enum {
  EV_UP,
  EV_DOWN,
  EV_SELECT,
  EV_BACK,
  EV_BACK_LONG,
  EV_TICK
} Event;

typedef enum {
  BOOK_UNREAD,
  BOOK_STARTED,
  BOOK_COMPLETED
} BookColorState;

/* What the watch caches per book. In SP1 this is filled from seed.c;
   in SP2 it arrives digested from the phone over AppMessage. */
typedef struct {
  char id[12];
  char title[48];
  int current_page;
  int total_pages;
  int pph_x100;   /* pages/hour * 100 */
  int hours_x100; /* total hours * 100 */
  BookColorState color;
  bool pph_is_estimate;
  bool favorite;
} DigestBook;

typedef struct {
  char id[16];
  char book_id[12];
  int start_page;
  int end_page;
  int pages;
  int duration_seconds;
} Session;

/* A completed session waiting in the watch's persistent queue for the
   phone to ACK it (SP2). No absolute timestamp — the phone stamps
   created_at on arrival. */
typedef struct {
  char id[16];
  char book_id[12];
  int start_page;
  int end_page;
  int duration_seconds;
} QueuedSession;

#endif
