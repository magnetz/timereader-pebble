#include <pebble.h>
#include "store.h"

/* Packed on-disk layout for the recovery session. Kept well under
   PERSIST_DATA_MAX_LENGTH (256 B on basalt). */
typedef struct {
  int32_t state;
  int32_t start_page;
  int32_t elapsed_seconds;
} SessionBlob;

void store_save_session(AppState state, int start_page, int elapsed_seconds) {
  SessionBlob blob = {
    .state = (int32_t)state,
    .start_page = (int32_t)start_page,
    .elapsed_seconds = (int32_t)elapsed_seconds,
  };
  persist_write_data(STORE_KEY_CUR_SESSION, &blob, sizeof(blob));
}

void store_clear_session(void) {
  if (persist_exists(STORE_KEY_CUR_SESSION)) {
    persist_delete(STORE_KEY_CUR_SESSION);
  }
}

PersistedSession store_load_session(void) {
  PersistedSession out = {0};
  if (!persist_exists(STORE_KEY_CUR_SESSION)) {
    out.present = false;
    return out;
  }
  SessionBlob blob = {0};
  int read = persist_read_data(STORE_KEY_CUR_SESSION, &blob, sizeof(blob));
  if (read < (int)sizeof(blob)) {
    out.present = false;
    return out;
  }
  out.present = true;
  out.state = (AppState)blob.state;
  out.start_page = blob.start_page;
  out.elapsed_seconds = blob.elapsed_seconds;
  return out;
}

void store_save_selected_book(int index) {
  persist_write_int(STORE_KEY_SELECTED_BOOK, index);
}

int store_load_selected_book(void) {
  if (!persist_exists(STORE_KEY_SELECTED_BOOK)) {
    return 0;
  }
  return persist_read_int(STORE_KEY_SELECTED_BOOK);
}
