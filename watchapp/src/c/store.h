#ifndef TR_STORE_H
#define TR_STORE_H

#include "model.h"

/* Persistent-storage keys (see spec "Persistent storage orologio").
   SP1 uses the recovery-session and selected-book subset only. */
#define STORE_KEY_CUR_SESSION 1
#define STORE_KEY_SELECTED_BOOK 2

typedef struct {
  AppState state;
  int start_page;
  int elapsed_seconds;
  bool present;
} PersistedSession;

void store_save_session(AppState state, int start_page, int elapsed_seconds);
void store_clear_session(void);
PersistedSession store_load_session(void);

void store_save_selected_book(int index);
int store_load_selected_book(void); /* 0 if unset */

/* --- SP2: digested-book cache with atomic shadow commit --- */
int  store_books_count(void);
int  store_books_load(DigestBook *out, int max);   /* fills out[0..count); returns count */
void store_shadow_begin(int count);                /* clears shadow, records expected count */
void store_shadow_put(int index, const DigestBook *b);
void store_shadow_commit(void);                    /* shadow -> cache, then clears shadow */
void store_shadow_discard(void);

/* --- SP2: persistent completed-session queue --- */
int  store_queue_count(void);
int  store_queue_load(QueuedSession *out, int max);   /* oldest first; returns count */
bool store_queue_push(const QueuedSession *s);        /* false if full (STORE_MAX_QUEUE) */
void store_queue_remove(const char *id);              /* drop the entry with this id, compact */
int  store_next_session_seq(void);                    /* monotonic counter for unique session ids */

#endif
