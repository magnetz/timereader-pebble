#include "seed.h"
#include "store.h"
#include "store_core.h"

/* SP1 hard-coded books are gone: the library now comes digested from the
   phone over AppMessage and lives in the persistent cache. */
static DigestBook s_books[STORE_MAX_BOOKS];
static int s_count = -1;

void seed_reload(void) {
  s_count = store_books_load(s_books, STORE_MAX_BOOKS);
}

const DigestBook *seed_books(int *count_out) {
  if (s_count < 0) seed_reload();
  *count_out = s_count;
  return s_books;
}
