#include "sync_core.h"

#include <string.h>

static void copy_id(char *dst, const char *src) {
  strncpy(dst, src ? src : "", 15);
  dst[15] = 0;
}

void sync_core_init(SyncCore *c, const SyncStore *s, void *sctx,
                    const SyncTransport *t, void *tctx,
                    void (*on_books_changed)(void *), void *ui_ctx) {
  memset(c, 0, sizeof(*c));
  c->store = s; c->store_ctx = sctx;
  c->tx = t; c->tx_ctx = tctx;
  c->on_books_changed = on_books_changed;
  c->ui_ctx = ui_ctx;
}

void sync_core_snapshot_begin(SyncCore *c, int count) {
  if (c->snapshot_active) {
    c->store->shadow_discard(c->store_ctx);
  }
  c->snapshot_active = true;
  c->snapshot_expected = count;
  c->snapshot_seen = 0;
  c->store->shadow_begin(c->store_ctx, count);
}

void sync_core_snapshot_book(SyncCore *c, int idx, const DigestBook *b) {
  if (!c->snapshot_active) return;
  c->store->shadow_put(c->store_ctx, idx, b);
  c->snapshot_seen++;
}

void sync_core_snapshot_end(SyncCore *c) {
  if (!c->snapshot_active) return;
  c->store->shadow_commit(c->store_ctx);
  c->snapshot_active = false;
  if (c->on_books_changed) c->on_books_changed(c->ui_ctx);
  sync_core_drain(c);
}

void sync_core_snapshot_abort(SyncCore *c) {
  if (!c->snapshot_active) return;
  c->store->shadow_discard(c->store_ctx);
  c->snapshot_active = false;
}

void sync_core_drain(SyncCore *c) {
  if (c->awaiting_ack || c->snapshot_active) return;
  QueuedSession head;
  if (!c->store->queue_head(c->store_ctx, &head)) return;
  if (c->tx->send_session(c->tx_ctx, &head)) {
    c->awaiting_ack = true;
    copy_id(c->inflight_id, head.id);
  }
}

void sync_core_enqueue(SyncCore *c, const QueuedSession *s) {
  c->store->queue_push(c->store_ctx, s);
  if (!c->awaiting_ack) sync_core_drain(c);
}

void sync_core_on_send_result(SyncCore *c, bool ok) {
  if (!ok) {
    /* let the next drain re-send the head */
    c->awaiting_ack = false;
    c->inflight_id[0] = 0;
  }
}

void sync_core_on_session_ack(SyncCore *c, const char *id) {
  if (!id) return;
  c->store->queue_remove(c->store_ctx, id);
  if (strcmp(c->inflight_id, id) == 0) {
    c->awaiting_ack = false;
    c->inflight_id[0] = 0;
  }
  sync_core_drain(c);
}

void sync_core_on_retract_ack(SyncCore *c, const char *id) {
  if (id && strcmp(c->pending_retract, id) == 0) {
    c->pending_retract[0] = 0;
  }
}

void sync_core_retract(SyncCore *c, const char *id) {
  if (!id) return;
  if (c->store->queue_contains(c->store_ctx, id)) {
    c->store->queue_remove(c->store_ctx, id);
    if (strcmp(c->inflight_id, id) == 0) {
      c->awaiting_ack = false;
      c->inflight_id[0] = 0;
    }
    return;
  }
  /* already ACKed and gone from the queue -> tell the phone */
  if (c->tx->send_retract(c->tx_ctx, id)) {
    copy_id(c->pending_retract, id);
  }
}
