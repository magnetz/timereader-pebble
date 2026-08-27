#ifndef TR_STORE_CORE_H
#define TR_STORE_CORE_H

/* Pure serialisation + key-layout math for the watch's persistent book
   cache and session queue. No <pebble.h> — host-testable. store.c wraps
   these with persist_* calls. */

#include "model.h"

#define STORE_MAX_BOOKS 24
#define STORE_MAX_QUEUE 50

/* Persist key layout. Small scalar keys 1..7; record arrays on disjoint
   numbered ranges (Pebble allows 256 keys per app). */
#define STORE_KEY_BOOKS_COUNT 3
#define STORE_KEY_SHADOW_COUNT 4
#define STORE_KEY_QUEUE_COUNT 5
#define STORE_KEY_SESSION_SEQ 6
#define STORE_KEY_SCHEMA_VERSION 7
#define STORE_KEY_BOOKS_BASE 100
#define STORE_KEY_SHADOW_BASE 140
#define STORE_KEY_QUEUE_BASE 180

/* Fixed-width little-endian packing so a record round-trips through a
   persist blob regardless of struct layout. Return bytes written / 1 on
   success, 0 on overflow or bad input. */
int store_core_pack_book(const DigestBook *b, unsigned char *buf, int buf_size);
int store_core_unpack_book(const unsigned char *buf, int len, DigestBook *out);
int store_core_pack_session(const QueuedSession *s, unsigned char *buf, int buf_size);
int store_core_unpack_session(const unsigned char *buf, int len, QueuedSession *out);

/* Key helpers. Return -1 if index is out of range. */
int store_core_book_key(int index);
int store_core_shadow_key(int index);
int store_core_queue_key(int index);

/* Array compaction used by the queue-remove path. Removes remove_index,
   shifts the tail down, returns the new count. */
int store_core_queue_drop(QueuedSession *q, int n, int remove_index);

/* id[12] + title[48] + 4*int32 + 3*uint8 */
#define STORE_CORE_BOOK_BYTES 79
/* id[16] + book_id[12] + 3*int32 */
#define STORE_CORE_SESSION_BYTES 40

#endif
