#include <pebble.h>
#include <string.h>
#include "store.h"
#include "store_core.h"

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

/* --- digested-book cache + shadow ------------------------------------- */

int store_books_count(void) {
  return persist_exists(STORE_KEY_BOOKS_COUNT) ? persist_read_int(STORE_KEY_BOOKS_COUNT) : 0;
}

int store_books_load(DigestBook *out, int max) {
  int n = store_books_count();
  if (n > max) n = max;
  if (n > STORE_MAX_BOOKS) n = STORE_MAX_BOOKS;
  int got = 0;
  for (int i = 0; i < n; i++) {
    int key = store_core_book_key(i);
    if (key < 0 || !persist_exists(key)) continue;
    unsigned char buf[STORE_CORE_BOOK_BYTES + 8];
    int len = persist_read_data(key, buf, sizeof(buf));
    if (len >= STORE_CORE_BOOK_BYTES && store_core_unpack_book(buf, len, &out[got])) got++;
  }
  return got;
}

void store_shadow_discard(void) {
  for (int i = 0; i < STORE_MAX_BOOKS; i++) {
    int key = store_core_shadow_key(i);
    if (persist_exists(key)) persist_delete(key);
  }
  if (persist_exists(STORE_KEY_SHADOW_COUNT)) persist_delete(STORE_KEY_SHADOW_COUNT);
}

void store_shadow_begin(int count) {
  store_shadow_discard();
  if (count > STORE_MAX_BOOKS) count = STORE_MAX_BOOKS;
  persist_write_int(STORE_KEY_SHADOW_COUNT, count);
}

void store_shadow_put(int index, const DigestBook *b) {
  int key = store_core_shadow_key(index);
  if (key < 0) return;
  unsigned char buf[STORE_CORE_BOOK_BYTES + 8];
  int n = store_core_pack_book(b, buf, sizeof(buf));
  if (n > 0) persist_write_data(key, buf, n);
}

void store_shadow_commit(void) {
  int count = persist_exists(STORE_KEY_SHADOW_COUNT) ? persist_read_int(STORE_KEY_SHADOW_COUNT) : 0;
  if (count > STORE_MAX_BOOKS) count = STORE_MAX_BOOKS;
  for (int i = 0; i < STORE_MAX_BOOKS; i++) {
    int ck = store_core_book_key(i), sk = store_core_shadow_key(i);
    if (i < count && persist_exists(sk)) {
      unsigned char buf[STORE_CORE_BOOK_BYTES + 8];
      int len = persist_read_data(sk, buf, sizeof(buf));
      persist_write_data(ck, buf, len);
    } else if (persist_exists(ck)) {
      persist_delete(ck);
    }
  }
  persist_write_int(STORE_KEY_BOOKS_COUNT, count);
  store_shadow_discard();
}

/* --- completed-session queue ---------------------------------------- */

int store_queue_count(void) {
  return persist_exists(STORE_KEY_QUEUE_COUNT) ? persist_read_int(STORE_KEY_QUEUE_COUNT) : 0;
}

static bool queue_read_at(int i, QueuedSession *out) {
  int key = store_core_queue_key(i);
  if (key < 0 || !persist_exists(key)) return false;
  unsigned char buf[STORE_CORE_SESSION_BYTES + 8];
  int len = persist_read_data(key, buf, sizeof(buf));
  return len >= STORE_CORE_SESSION_BYTES && store_core_unpack_session(buf, len, out);
}

bool store_queue_head(QueuedSession *out) {
  if (store_queue_count() <= 0) return false;
  return queue_read_at(0, out);
}

bool store_queue_contains(const char *id) {
  int n = store_queue_count();
  QueuedSession s;
  for (int i = 0; i < n; i++) {
    if (queue_read_at(i, &s) && strcmp(s.id, id) == 0) return true;
  }
  return false;
}

bool store_queue_push(const QueuedSession *s) {
  int n = store_queue_count();
  if (n >= STORE_MAX_QUEUE) return false;
  unsigned char buf[STORE_CORE_SESSION_BYTES + 8];
  int len = store_core_pack_session(s, buf, sizeof(buf));
  if (len <= 0) return false;
  persist_write_data(store_core_queue_key(n), buf, len);
  persist_write_int(STORE_KEY_QUEUE_COUNT, n + 1);
  return true;
}

void store_queue_remove(const char *id) {
  /* static, not stack: STORE_MAX_QUEUE * sizeof(QueuedSession) is 2 KB,
     which would blow the small Pebble app stack. */
  static QueuedSession s_scratch[STORE_MAX_QUEUE];
  int n = store_queue_count();
  if (n > STORE_MAX_QUEUE) n = STORE_MAX_QUEUE;
  int got = 0;
  for (int i = 0; i < n; i++) {
    if (queue_read_at(i, &s_scratch[got])) got++;
  }
  int idx = -1;
  for (int i = 0; i < got; i++) {
    if (strcmp(s_scratch[i].id, id) == 0) { idx = i; break; }
  }
  if (idx < 0) return;
  int rest = store_core_queue_drop(s_scratch, got, idx);
  for (int i = 0; i < rest; i++) {
    unsigned char buf[STORE_CORE_SESSION_BYTES + 8];
    int len = store_core_pack_session(&s_scratch[i], buf, sizeof(buf));
    persist_write_data(store_core_queue_key(i), buf, len);
  }
  if (persist_exists(store_core_queue_key(rest))) persist_delete(store_core_queue_key(rest));
  persist_write_int(STORE_KEY_QUEUE_COUNT, rest);
}

int store_next_session_seq(void) {
  int v = persist_exists(STORE_KEY_SESSION_SEQ) ? persist_read_int(STORE_KEY_SESSION_SEQ) : 0;
  persist_write_int(STORE_KEY_SESSION_SEQ, v + 1);
  return v;
}
