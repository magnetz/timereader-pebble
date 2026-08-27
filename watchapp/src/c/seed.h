#ifndef TR_SEED_H
#define TR_SEED_H

#include "model.h"

/* Hard-coded book list for SP1. SP2 replaces the source with the
   AppMessage-fed cache; the signature stays the same. */
const DigestBook *seed_books(int *count_out);

#endif
