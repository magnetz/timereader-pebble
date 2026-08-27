#ifndef TR_STRINGS_H
#define TR_STRINGS_H

/* Minimal UI localisation. Default language is English; Italian is used
   when the watch's system locale starts with "it". Format strings keep
   their %d/%s placeholders — the id names say the argument order. */

typedef enum {
  STR_NO_BOOKS,          /* two lines */
  STR_START_PAGE,
  STR_END_PAGE,
  STR_END_LT_START,
  STR_PAGES_UNKNOWN,     /* two lines */
  STR_PAUSE,
  STR_PAGE_FMT,          /* "%d" current page */
  STR_PAGES_FMT,         /* "%d" pages read */
  STR_PPH_FMT,           /* "%d.%d" rate, "%s" estimate suffix */
  STR_EST,               /* estimate suffix, leading space */
  STR_TIME_TOTAL_FMT,    /* "%d.%02d" hours */
  STR_LEFT_H_FMT,        /* "%d" pages, "%d" hours, "%s" suffix */
  STR_LEFT_MIN_FMT,      /* "%d" pages, "%d" minutes, "%s" suffix */
  STR_MENU_SAVE,
  STR_MENU_EXIT,
  STR_MENU_CANCEL,
  STR_EXIT_CONFIRM,
  STR_EXIT_HINT,         /* two lines */
  STR_SESSION_SAVED,
  STR_SUMMARY_HINT,
  STR__COUNT
} StringId;

const char *S(StringId id);

#endif
