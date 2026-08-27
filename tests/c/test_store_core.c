#include "store_core.h"
#include "test.h"
#include <string.h>

void t_book_roundtrip(void) {
  DigestBook b; memset(&b, 0, sizeof(b));
  strcpy(b.id, "b_abc"); strcpy(b.title, "Un Titolo Lungo Ma Non Troppo");
  b.current_page = 123; b.total_pages = 456; b.pph_x100 = 4550; b.hours_x100 = 268;
  b.color = BOOK_STARTED; b.pph_is_estimate = true; b.favorite = false;

  unsigned char buf[256];
  int n = store_core_pack_book(&b, buf, sizeof(buf));
  CHECK(n > 0, "packed");
  DigestBook out; memset(&out, 0xAA, sizeof(out));
  CHECK(store_core_unpack_book(buf, n, &out), "unpacked");
  CHECK_EQ_STR(out.id, b.id, "");
  CHECK_EQ_STR(out.title, b.title, "");
  CHECK_EQ_INT(out.current_page, 123, "");
  CHECK_EQ_INT(out.total_pages, 456, "");
  CHECK_EQ_INT(out.pph_x100, 4550, "");
  CHECK_EQ_INT(out.color, BOOK_STARTED, "");
  CHECK_EQ_INT(out.pph_is_estimate, 1, "");
  CHECK_EQ_INT(out.favorite, 0, "");
}

void t_book_pack_fits_persist_limit(void) {
  DigestBook b; memset(&b, 0, sizeof(b));
  memset(b.title, 'x', sizeof(b.title) - 1);
  strcpy(b.id, "b_00000000");
  unsigned char buf[256];
  int n = store_core_pack_book(&b, buf, sizeof(buf));
  CHECK(n > 0 && n <= 256, "a maxed record still fits one persist key");
}

void t_session_roundtrip(void) {
  QueuedSession s; memset(&s, 0, sizeof(s));
  strcpy(s.id, "w12_600"); strcpy(s.book_id, "b_abc");
  s.start_page = 49; s.end_page = 61; s.duration_seconds = 733;
  unsigned char buf[128];
  int n = store_core_pack_session(&s, buf, sizeof(buf));
  QueuedSession out; memset(&out, 0, sizeof(out));
  CHECK(store_core_unpack_session(buf, n, &out), "");
  CHECK_EQ_STR(out.id, "w12_600", "");
  CHECK_EQ_STR(out.book_id, "b_abc", "");
  CHECK_EQ_INT(out.start_page, 49, "");
  CHECK_EQ_INT(out.end_page, 61, "");
  CHECK_EQ_INT(out.duration_seconds, 733, "");
}

void t_key_layout_is_disjoint(void) {
  CHECK(store_core_book_key(0) != store_core_shadow_key(0), "");
  CHECK(store_core_shadow_key(STORE_MAX_BOOKS - 1) < store_core_queue_key(0), "no overlap");
  CHECK(store_core_book_key(STORE_MAX_BOOKS) == -1, "out of range guarded");
}

void t_queue_compaction_shifts_tail_down(void) {
  QueuedSession q[3];
  for (int i = 0; i < 3; i++) { memset(&q[i], 0, sizeof(q[i])); q[i].start_page = i; }
  int n = store_core_queue_drop(q, 3, 1);
  CHECK_EQ_INT(n, 2, "");
  CHECK_EQ_INT(q[0].start_page, 0, "");
  CHECK_EQ_INT(q[1].start_page, 2, "tail shifted down");
}

TEST_BEGIN()
  t_book_roundtrip();
  t_book_pack_fits_persist_limit();
  t_session_roundtrip();
  t_key_layout_is_disjoint();
  t_queue_compaction_shifts_tail_down();
TEST_END()
