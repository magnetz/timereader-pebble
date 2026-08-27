#include "digit_entry.h"

void digit_entry_init(DigitEntry *e, int prefill) {
  if (prefill < 0) prefill = 0;
  if (prefill > 9999) prefill = 9999;
  e->digits[0] = (prefill / 1000) % 10;
  e->digits[1] = (prefill / 100) % 10;
  e->digits[2] = (prefill / 10) % 10;
  e->digits[3] = prefill % 10;
  e->cursor = 0;
}

int digit_entry_value(const DigitEntry *e) {
  return e->digits[0] * 1000 + e->digits[1] * 100 + e->digits[2] * 10 + e->digits[3];
}

void digit_entry_up(DigitEntry *e) {
  e->digits[e->cursor] = (e->digits[e->cursor] + 1) % 10;
}

void digit_entry_down(DigitEntry *e) {
  e->digits[e->cursor] = (e->digits[e->cursor] + 9) % 10;
}

bool digit_entry_select(DigitEntry *e) {
  if (e->cursor >= 3) return true;
  e->cursor++;
  return false;
}

bool digit_entry_back(DigitEntry *e) {
  if (e->cursor == 0) return true;
  e->cursor--;
  return false;
}
