#ifndef TR_FAKE_SYNC_ENV_H
#define TR_FAKE_SYNC_ENV_H

/* In-memory store + transport fakes for sync_core tests. */

#include "sync_core.h"
#include "store_core.h"
#include <string.h>

typedef struct {
  DigestBook cache[STORE_MAX_BOOKS];
  int cache_count;
  DigestBook shadow[STORE_MAX_BOOKS];
  int shadow_count;      /* records actually put */
  int shadow_expected;

  QueuedSession queue[STORE_MAX_QUEUE];
  int queue_count;

  QueuedSession sent[64];
  int sent_count;
  char sent_retracts[16][16];
  int sent_retract_count;

  int on_books_changed_calls;
  int tx_offline;
} FakeEnv;

/* --- store vtable --- */
static void fe_shadow_begin(void *ctx, int count) {
  FakeEnv *e = ctx;
  e->shadow_count = 0;
  e->shadow_expected = count;
}
static void fe_shadow_put(void *ctx, int idx, const DigestBook *b) {
  FakeEnv *e = ctx;
  if (idx >= 0 && idx < STORE_MAX_BOOKS) {
    e->shadow[idx] = *b;
    if (idx + 1 > e->shadow_count) e->shadow_count = idx + 1;
  }
}
static void fe_shadow_commit(void *ctx) {
  FakeEnv *e = ctx;
  int n = e->shadow_expected;
  if (n > STORE_MAX_BOOKS) n = STORE_MAX_BOOKS;
  for (int i = 0; i < n; i++) e->cache[i] = e->shadow[i];
  e->cache_count = n;
  e->shadow_count = 0;
  e->shadow_expected = 0;
}
static void fe_shadow_discard(void *ctx) {
  FakeEnv *e = ctx;
  e->shadow_count = 0;
  e->shadow_expected = 0;
}
static bool fe_queue_head(void *ctx, QueuedSession *out) {
  FakeEnv *e = ctx;
  if (e->queue_count <= 0) return false;
  *out = e->queue[0];
  return true;
}
static bool fe_queue_contains(void *ctx, const char *id) {
  FakeEnv *e = ctx;
  for (int i = 0; i < e->queue_count; i++) {
    if (strcmp(e->queue[i].id, id) == 0) return true;
  }
  return false;
}
static bool fe_queue_push(void *ctx, const QueuedSession *s) {
  FakeEnv *e = ctx;
  if (e->queue_count >= STORE_MAX_QUEUE) return false;
  e->queue[e->queue_count++] = *s;
  return true;
}
static void fe_queue_remove(void *ctx, const char *id) {
  FakeEnv *e = ctx;
  int idx = -1;
  for (int i = 0; i < e->queue_count; i++) {
    if (strcmp(e->queue[i].id, id) == 0) { idx = i; break; }
  }
  if (idx < 0) return;
  e->queue_count = store_core_queue_drop(e->queue, e->queue_count, idx);
}

/* --- transport vtable --- */
static bool fe_send_session(void *ctx, const QueuedSession *s) {
  FakeEnv *e = ctx;
  if (e->tx_offline) return false;
  e->sent[e->sent_count++] = *s;
  return true;
}
static bool fe_send_retract(void *ctx, const char *id) {
  FakeEnv *e = ctx;
  if (e->tx_offline) return false;
  strncpy(e->sent_retracts[e->sent_retract_count], id, 15);
  e->sent_retracts[e->sent_retract_count][15] = 0;
  e->sent_retract_count++;
  return true;
}

static const SyncStore FE_STORE = {
  fe_shadow_begin, fe_shadow_put, fe_shadow_commit, fe_shadow_discard,
  fe_queue_head, fe_queue_contains, fe_queue_push, fe_queue_remove,
};
static const SyncTransport FE_TX = { fe_send_session, fe_send_retract };

static void fe_on_books_changed(void *ui_ctx) {
  FakeEnv *e = ui_ctx;
  e->on_books_changed_calls++;
}

static void fake_env_init(FakeEnv *e) {
  memset(e, 0, sizeof(*e));
}

static void fake_env_wire(FakeEnv *e, SyncCore *c) {
  sync_core_init(c, &FE_STORE, e, &FE_TX, e, fe_on_books_changed, e);
}

static void fake_env_seed_cache(FakeEnv *e, int n) {
  if (n > STORE_MAX_BOOKS) n = STORE_MAX_BOOKS;
  for (int i = 0; i < n; i++) {
    memset(&e->cache[i], 0, sizeof(e->cache[i]));
    e->cache[i].id[0] = 'a' + i;
  }
  e->cache_count = n;
}

#endif
