#ifndef TR_DIGIT_ENTRY_H
#define TR_DIGIT_ENTRY_H

#include <stdbool.h>

typedef struct {
  int digits[4];
  int cursor;
} DigitEntry;

void digit_entry_init(DigitEntry *e, int prefill);  /* prefill 0..9999 -> digits, cursor=0 */
int  digit_entry_value(const DigitEntry *e);         /* digits -> 0..9999 */
void digit_entry_up(DigitEntry *e);                  /* active digit +1 mod 10 */
void digit_entry_down(DigitEntry *e);                /* active digit -1 mod 10 */
bool digit_entry_select(DigitEntry *e);              /* advance cursor; true if that was the last digit */
bool digit_entry_back(DigitEntry *e);                /* retreat cursor; true if cursor was already 0 (cancel) */

#endif
