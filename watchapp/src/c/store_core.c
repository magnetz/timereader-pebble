#include "store_core.h"

#include <string.h>

static void put32(unsigned char *p, int v) {
  unsigned u = (unsigned)v;
  p[0] = u & 0xff;
  p[1] = (u >> 8) & 0xff;
  p[2] = (u >> 16) & 0xff;
  p[3] = (u >> 24) & 0xff;
}

static int get32(const unsigned char *p) {
  unsigned u = (unsigned)p[0] | ((unsigned)p[1] << 8) |
               ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
  return (int)u;
}

int store_core_pack_book(const DigestBook *b, unsigned char *buf, int buf_size) {
  if (!b || !buf || buf_size < STORE_CORE_BOOK_BYTES) return 0;
  unsigned char *p = buf;
  memset(p, 0, STORE_CORE_BOOK_BYTES);
  memcpy(p, b->id, sizeof(b->id));            p += sizeof(b->id);      /* 12 */
  memcpy(p, b->title, sizeof(b->title));      p += sizeof(b->title);   /* 48 */
  put32(p, b->current_page);  p += 4;
  put32(p, b->total_pages);   p += 4;
  put32(p, b->pph_x100);      p += 4;
  put32(p, b->hours_x100);    p += 4;
  *p++ = (unsigned char)b->color;
  *p++ = b->pph_is_estimate ? 1 : 0;
  *p++ = b->favorite ? 1 : 0;
  return STORE_CORE_BOOK_BYTES;
}

int store_core_unpack_book(const unsigned char *buf, int len, DigestBook *out) {
  if (!buf || !out || len < STORE_CORE_BOOK_BYTES) return 0;
  const unsigned char *p = buf;
  memset(out, 0, sizeof(*out));
  memcpy(out->id, p, sizeof(out->id));       p += sizeof(out->id);
  out->id[sizeof(out->id) - 1] = 0;
  memcpy(out->title, p, sizeof(out->title)); p += sizeof(out->title);
  out->title[sizeof(out->title) - 1] = 0;
  out->current_page = get32(p); p += 4;
  out->total_pages = get32(p);  p += 4;
  out->pph_x100 = get32(p);     p += 4;
  out->hours_x100 = get32(p);   p += 4;
  out->color = (BookColorState)(*p++);
  out->pph_is_estimate = (*p++ != 0);
  out->favorite = (*p++ != 0);
  return 1;
}

int store_core_pack_session(const QueuedSession *s, unsigned char *buf, int buf_size) {
  if (!s || !buf || buf_size < STORE_CORE_SESSION_BYTES) return 0;
  unsigned char *p = buf;
  memset(p, 0, STORE_CORE_SESSION_BYTES);
  memcpy(p, s->id, sizeof(s->id));           p += sizeof(s->id);       /* 16 */
  memcpy(p, s->book_id, sizeof(s->book_id)); p += sizeof(s->book_id);  /* 12 */
  put32(p, s->start_page);      p += 4;
  put32(p, s->end_page);        p += 4;
  put32(p, s->duration_seconds); p += 4;
  return STORE_CORE_SESSION_BYTES;
}

int store_core_unpack_session(const unsigned char *buf, int len, QueuedSession *out) {
  if (!buf || !out || len < STORE_CORE_SESSION_BYTES) return 0;
  const unsigned char *p = buf;
  memset(out, 0, sizeof(*out));
  memcpy(out->id, p, sizeof(out->id));       p += sizeof(out->id);
  out->id[sizeof(out->id) - 1] = 0;
  memcpy(out->book_id, p, sizeof(out->book_id)); p += sizeof(out->book_id);
  out->book_id[sizeof(out->book_id) - 1] = 0;
  out->start_page = get32(p);      p += 4;
  out->end_page = get32(p);        p += 4;
  out->duration_seconds = get32(p); p += 4;
  return 1;
}

int store_core_book_key(int index) {
  if (index < 0 || index >= STORE_MAX_BOOKS) return -1;
  return STORE_KEY_BOOKS_BASE + index;
}
int store_core_shadow_key(int index) {
  if (index < 0 || index >= STORE_MAX_BOOKS) return -1;
  return STORE_KEY_SHADOW_BASE + index;
}
int store_core_queue_key(int index) {
  if (index < 0 || index >= STORE_MAX_QUEUE) return -1;
  return STORE_KEY_QUEUE_BASE + index;
}

int store_core_queue_drop(QueuedSession *q, int n, int remove_index) {
  if (!q || remove_index < 0 || remove_index >= n) return n;
  for (int i = remove_index; i < n - 1; i++) {
    q[i] = q[i + 1];
  }
  return n - 1;
}
