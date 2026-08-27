#!/usr/bin/env bash
# Runs every tests/js/test_*.mjs with Node's built-in test runner.
# jsdom is installed locally (pinned): npm i --no-save jsdom@25
set -euo pipefail
cd "$(dirname "$0")"
shopt -s nullglob
files=(test_*.mjs)
if [ ${#files[@]} -eq 0 ]; then
  echo "no test_*.mjs files yet"
  exit 0
fi
node --test "${files[@]}"
