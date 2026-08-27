#ifndef TR_SEED_H
#define TR_SEED_H

#include "model.h"

/* Serves the SP2 sync cache. Returns a pointer to a static buffer filled
   from store_books_load(); *count_out is 0 when the phone has never
   synced (the state machine then shows APP_NO_BOOKS). */
const DigestBook *seed_books(int *count_out);

/* Re-read the cache after a snapshot commit. */
void seed_reload(void);

#endif
