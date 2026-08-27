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

#endif
