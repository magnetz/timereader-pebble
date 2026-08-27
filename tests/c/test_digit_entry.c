#include "digit_entry.h"
#include "test.h"

void t_prefill_and_value(void) {
  DigitEntry e; digit_entry_init(&e, 137);
  CHECK_EQ_INT(e.digits[0], 0, "");
  CHECK_EQ_INT(e.digits[1], 1, "");
  CHECK_EQ_INT(e.digits[2], 3, "");
  CHECK_EQ_INT(e.digits[3], 7, "");
  CHECK_EQ_INT(digit_entry_value(&e), 137, "");
  CHECK_EQ_INT(e.cursor, 0, "");
}

void t_up_wraps(void) {
  DigitEntry e; digit_entry_init(&e, 0);
  digit_entry_down(&e);              /* 0 -> 9 on digit 0 */
  CHECK_EQ_INT(e.digits[0], 9, "");
  digit_entry_up(&e);               /* 9 -> 0 */
  CHECK_EQ_INT(e.digits[0], 0, "");
}

void t_select_advances_and_completes(void) {
  DigitEntry e; digit_entry_init(&e, 0);
  CHECK(!digit_entry_select(&e), "cursor 0->1");
  CHECK(!digit_entry_select(&e), "1->2");
  CHECK(!digit_entry_select(&e), "2->3");
  CHECK(digit_entry_select(&e), "last digit confirmed");
}

void t_back_retreats_then_cancels(void) {
  DigitEntry e; digit_entry_init(&e, 0);
  digit_entry_select(&e); digit_entry_select(&e);   /* cursor at 2 */
  CHECK(!digit_entry_back(&e), "retreat 2->1");
  CHECK(!digit_entry_back(&e), "retreat 1->0");
  CHECK(digit_entry_back(&e), "at 0 -> cancel");
  CHECK_EQ_INT(digit_entry_value(&e), 0, "digits unchanged by back");
}

TEST_BEGIN()
  t_prefill_and_value();
  t_up_wraps();
  t_select_advances_and_completes();
  t_back_retreats_then_cancels();
TEST_END()
