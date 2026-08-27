#!/usr/bin/env bash
# Compiles and runs every tests/c/test_*.c on the host with cc.
# No Pebble SDK involved: only the hardware-agnostic logic files
# (digit_entry, session, state_machine) are host-safe.
set -euo pipefail
cd "$(dirname "$0")"

SRC=../../watchapp/src/c
CFLAGS="-std=c11 -Wall -Wextra -Werror -I$SRC"

IMPL=""
for f in digit_entry session state_machine; do
  [ -f "$SRC/$f.c" ] && IMPL="$IMPL $SRC/$f.c"
done

shopt -s nullglob
tests=(test_*.c)
if [ ${#tests[@]} -eq 0 ]; then
  echo "no test_*.c files yet"
  exit 0
fi

fail=0
for t in "${tests[@]}"; do
  bin="/tmp/tr_${t%.c}"
  # shellcheck disable=SC2086
  cc $CFLAGS -o "$bin" "$t" $IMPL
  if ! "$bin"; then fail=1; fi
done
exit $fail
