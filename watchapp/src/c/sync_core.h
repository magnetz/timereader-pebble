#ifndef TR_SYNC_CORE_H
#define TR_SYNC_CORE_H

/* Pure sync decision logic — no <pebble.h>. Talks to the world through a
   store vtable and a transport vtable the caller supplies. sync.c wires
   the real persist_* / app_message ones; the tests wire fakes. */

#include "model.h"
#include "store_core.h"   /* QueuedSession sizing + STORE_MAX_QUEUE; also pure */

typedef struct {
  void (*shadow_begin)(void *ctx, int count);
  void (*shadow_put)(void *ctx, int idx, const DigestBook *b);
  void (*shadow_commit)(void *ctx);
  void (*shadow_discard)(void *ctx);
  int  (*queue_load)(void *ctx, QueuedSession *out, int max);
  bool (*queue_push)(void *ctx, const QueuedSession *s);
  void (*queue_remove)(void *ctx, const char *id);
} SyncStore;

typedef struct {
  /* true if accepted for sending (exactly one message in flight) */
  bool (*send_session)(void *ctx, const QueuedSession *s);
  bool (*send_retract)(void *ctx, const char *id);
} SyncTransport;

typedef struct {
  const SyncStore *store;
  void *store_ctx;
  const SyncTransport *tx;
  void *tx_ctx;

  bool snapshot_active;
  int  snapshot_expected;
  int  snapshot_seen;

  bool awaiting_ack;
  char inflight_id[16];
  char pending_retract[16];   /* non-empty => waiting for RETRACT_ACK */

  void (*on_books_changed)(void *ui_ctx);
  void *ui_ctx;
} SyncCore;

void sync_core_init(SyncCore *c, const SyncStore *s, void *sctx,
                    const SyncTransport *t, void *tctx,
                    void (*on_books_changed)(void *), void *ui_ctx);

void sync_core_snapshot_begin(SyncCore *c, int count);
void sync_core_snapshot_book(SyncCore *c, int idx, const DigestBook *b);
void sync_core_snapshot_end(SyncCore *c);
void sync_core_snapshot_abort(SyncCore *c);

void sync_core_enqueue(SyncCore *c, const QueuedSession *s);
void sync_core_drain(SyncCore *c);
void sync_core_on_session_ack(SyncCore *c, const char *id);
void sync_core_on_retract_ack(SyncCore *c, const char *id);
void sync_core_retract(SyncCore *c, const char *id);
void sync_core_on_send_result(SyncCore *c, bool ok);

#endif
