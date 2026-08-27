#include <pebble.h>
#include <string.h>
#include "sync.h"
#include "sync_core.h"
#include "store.h"

static SyncCore s_core;
static void (*s_on_books_changed_ui)(void);

static void ui_books_changed(void *ctx) {
  (void)ctx;
  if (s_on_books_changed_ui) s_on_books_changed_ui();
}

/* --- SyncStore over store.c --- */
static void st_shadow_begin(void *c, int n) { (void)c; store_shadow_begin(n); }
static void st_shadow_put(void *c, int i, const DigestBook *b) { (void)c; store_shadow_put(i, b); }
static void st_shadow_commit(void *c) { (void)c; store_shadow_commit(); }
static void st_shadow_discard(void *c) { (void)c; store_shadow_discard(); }
static int  st_queue_load(void *c, QueuedSession *o, int m) { (void)c; return store_queue_load(o, m); }
static bool st_queue_push(void *c, const QueuedSession *s) { (void)c; return store_queue_push(s); }
static void st_queue_remove(void *c, const char *id) { (void)c; store_queue_remove(id); }

static const SyncStore STORE_VT = {
  st_shadow_begin, st_shadow_put, st_shadow_commit, st_shadow_discard,
  st_queue_load, st_queue_push, st_queue_remove,
};

/* --- SyncTransport over app_message --- */
static bool tx_send_session(void *c, const QueuedSession *s) {
  (void)c;
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return false;
  dict_write_cstring(it, MESSAGE_KEY_SESSION_ID, s->id);
  dict_write_cstring(it, MESSAGE_KEY_SESSION_BOOK_ID, s->book_id);
  dict_write_int32(it, MESSAGE_KEY_SESSION_START_PAGE, s->start_page);
  dict_write_int32(it, MESSAGE_KEY_SESSION_END_PAGE, s->end_page);
  dict_write_int32(it, MESSAGE_KEY_SESSION_DURATION_S, s->duration_seconds);
  return app_message_outbox_send() == APP_MSG_OK;
}
static bool tx_send_retract(void *c, const char *id) {
  (void)c;
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return false;
  dict_write_cstring(it, MESSAGE_KEY_SESSION_RETRACT_ID, id);
  return app_message_outbox_send() == APP_MSG_OK;
}
static const SyncTransport TX_VT = { tx_send_session, tx_send_retract };

static void inbox_received(DictionaryIterator *it, void *ctx) {
  (void)ctx;
  Tuple *t;
  if ((t = dict_find(it, MESSAGE_KEY_SNAPSHOT_BEGIN))) {
    sync_core_snapshot_begin(&s_core, t->value->int32);
    return;
  }
  if (dict_find(it, MESSAGE_KEY_SNAPSHOT_END)) {
    sync_core_snapshot_end(&s_core);
    return;
  }
  if ((t = dict_find(it, MESSAGE_KEY_BOOK_IDX))) {
    DigestBook b;
    memset(&b, 0, sizeof(b));
    int idx = t->value->int32;
    Tuple *x;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_ID)))        strncpy(b.id, x->value->cstring, sizeof(b.id) - 1);
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_TITLE)))     strncpy(b.title, x->value->cstring, sizeof(b.title) - 1);
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_COLOR)))     b.color = (BookColorState)x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_CUR_PAGE)))  b.current_page = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_TOT_PAGES))) b.total_pages = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_PPH_X100)))  b.pph_x100 = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_HOURS_X100)))b.hours_x100 = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_FLAGS))) {
      int f = x->value->int32;
      b.pph_is_estimate = (f & 1) != 0;
      b.favorite = (f & 2) != 0;
    }
    sync_core_snapshot_book(&s_core, idx, &b);
    return;
  }
  if ((t = dict_find(it, MESSAGE_KEY_SESSION_ACK_ID))) {
    sync_core_on_session_ack(&s_core, t->value->cstring);
    return;
  }
  if ((t = dict_find(it, MESSAGE_KEY_RETRACT_ACK_ID))) {
    sync_core_on_retract_ack(&s_core, t->value->cstring);
    return;
  }
}

static void outbox_sent(DictionaryIterator *it, void *ctx) {
  (void)it; (void)ctx;
  sync_core_on_send_result(&s_core, true);
}
static void outbox_failed(DictionaryIterator *it, AppMessageResult r, void *ctx) {
  (void)it; (void)r; (void)ctx;
  sync_core_on_send_result(&s_core, false);
}

void sync_init(void (*on_books_changed)(void)) {
  s_on_books_changed_ui = on_books_changed;
  sync_core_init(&s_core, &STORE_VT, NULL, &TX_VT, NULL, ui_books_changed, NULL);
  app_message_register_inbox_received(inbox_received);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(512, 128);
}

void sync_enqueue_session(const QueuedSession *s) { sync_core_enqueue(&s_core, s); }
void sync_retract_session(const char *id) { sync_core_retract(&s_core, id); }
void sync_drain(void) { sync_core_drain(&s_core); }
int  sync_books_into(DigestBook *out, int max) { return store_books_load(out, max); }
