#include "seed.h"

static const DigestBook BOOKS[] = {
  {.id = "b1", .title = "La Storia Infinita", .current_page = 48, .total_pages = 445,
   .pph_x100 = 3200, .hours_x100 = 150, .color = BOOK_STARTED,
   .pph_is_estimate = false, .favorite = true},
  {.id = "b2", .title = "Il Nome della Rosa", .current_page = 0, .total_pages = 620,
   .pph_x100 = 3000, .hours_x100 = 0, .color = BOOK_UNREAD,
   .pph_is_estimate = true, .favorite = false},
};

const DigestBook *seed_books(int *count_out) {
  *count_out = 2;
  return BOOKS;
}
