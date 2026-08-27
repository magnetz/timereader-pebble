#ifndef TR_SYNC_H
#define TR_SYNC_H

#include "model.h"

/* PebbleOS side of SP2 sync. Wires app_message + store.c into sync_core. */
void sync_init(void (*on_books_changed)(void));
void sync_enqueue_session(const QueuedSession *s);
void sync_retract_session(const char *id);
void sync_drain(void);
int  sync_books_into(DigestBook *out, int max);

#endif
