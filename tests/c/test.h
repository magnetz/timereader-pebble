#ifndef TR_TEST_H
#define TR_TEST_H

#include <stdio.h>
#include <string.h>

/* One test binary per test_*.c file (see run.sh), so file-scoped
   counters are fine and there is exactly one main() per link. */
static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
  tests_run++; \
  if (!(cond)) { \
    printf("FAIL: %s (%s:%d) %s\n", #cond, __FILE__, __LINE__, (msg)); \
    tests_failed++; \
  } \
} while (0)

#define CHECK_EQ_INT(a, e, msg) do { \
  tests_run++; \
  long _got = (long)(a), _want = (long)(e); \
  if (_got != _want) { \
    printf("FAIL: %s == %s | got %ld want %ld (%s:%d) %s\n", \
           #a, #e, _got, _want, __FILE__, __LINE__, (msg)); \
    tests_failed++; \
  } \
} while (0)

#define CHECK_EQ_STR(a, e, msg) do { \
  tests_run++; \
  if (strcmp((a), (e)) != 0) { \
    printf("FAIL: %s == %s | got \"%s\" want \"%s\" (%s:%d) %s\n", \
           #a, #e, (a), (e), __FILE__, __LINE__, (msg)); \
    tests_failed++; \
  } \
} while (0)

#define TEST_BEGIN() int main(void) {
#define TEST_END() \
  printf("%s: %d passed / %d failed\n", \
         __FILE__, tests_run - tests_failed, tests_failed); \
  return tests_failed ? 1 : 0; \
}

#endif
