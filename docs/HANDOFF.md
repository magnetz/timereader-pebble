# TimeReader Pebble — Handoff for the next agent

**Date:** 2026-08-27
**Branch:** `sp1-watchapp` (do not work on `main`)
**Repo:** `~/app/timereader-pebble`

## What this project is

A C rewrite of `~/app/m5/timereader` (a MicroPython/ESP32 reading timer)
for the **Pebble Time** (`basalt` platform). Two sub-projects:

- **SP1 — watchapp core** (in progress): the on-watch reading timer.
  Plan: `docs/superpowers/plans/2026-08-27-sp1-watchapp-core.md`.
- **SP2 — companion + Bluetooth sync** (spec only, not started):
  PebbleKit JS companion + static config page + AppMessage sync.
  Spec: `docs/superpowers/specs/2026-08-27-companion-bt-sync-design.md`.

Read both the plan and the spec before continuing. The original
MicroPython app at `~/app/m5/timereader/firmware/app/` is the behavioural
reference for the port.

## Progress so far

Executing the SP1 plan with the `superpowers:executing-plans` workflow
(TDD, bite-sized steps, one commit per task). **All tasks 1–14 are done
and committed.**

| Task | Deliverable | State |
|---|---|---|
| 1 | Toolchain + `watchapp/` scaffold | done |
| 2 | `model.h` + host C test harness | done |
| 3 | `digit_entry.c/.h` — 4-digit entry | done, 16 tests |
| 4 | `session.c/.h` — live timing + display stats + duration formatter | done, 19 tests |
| 5 | `state_machine.c/.h` — 4-button state machine | done, 53 tests |
| 6 | `seed.c/.h` + `store.c/.h` (persistent storage wrappers) | done |
| 7 | App shell — `main.c`, `ui_common.c/.h`, tick timer, central dispatch | done |
| 8 | `ui_list.c/.h` — book list (MenuLayer) + no-books window | done |
| 9 | `ui_detail.c/.h` — 3 info pages + page dots | done |
| 10 | `ui_digit.c/.h` — animated 4-digit entry + error flash | done |
| 11 | `ui_timer.c/.h` — StatusBar + ActionBar + PDC play/pause icons | done |
| 12 | `ui_endmenu.c/.h` — ActionMenu + exit-without-saving confirm modal | done |
| 13 | `ui_summary.c/.h` — summary, retract, crash-recovery persistence | done |
| 14 | Polish (animated back-nav) + `docs/on-device-checklist.md` | done |

**Design change during SP1 (user rule):** a new session prefills the
start page as `current_page + 1` (started book) or `1` (unread book);
pages read in a session are counted inclusively (`end - start + 1`). See
the Global Constraints note in the plan. Timer/summary show `h:mm:ss`
once a session passes an hour (`session_format_duration`).

## SP2 — companion + Bluetooth sync (done)

Plan: `docs/superpowers/plans/2026-08-27-sp2-companion-bt-sync.md`.
Branch `sp2-companion`. **Tasks 1–15 done and committed** (Task 16,
distribution docs, is the only remaining plan item).

| Area | Deliverable |
|---|---|
| `src/pkjs/datastore.js` | async localStorage seam (`tr_books` / `tr_sessions`), idempotent append, cascade delete |
| `src/pkjs/library.js` | Σpages/Σduration rate, global estimate + flag, `colorState`, `digestBook`, `computeSnapshot` |
| `src/pkjs/index.js` | `showConfiguration`/`webviewclosed` diff, `sendSnapshot` (one msg at a time), `SESSION`/`SESSION_RETRACT` handlers |
| `config-page/index.html` | stateless hash-in / `pebblejs://close`-out page: books, favourites, reorder, sessions tab, HTML escaping |
| `src/c/store_core.c` + `store.c` | fixed-width record packing, one persist key per book/queue-entry, atomic shadow→cache commit, session queue |
| `src/c/sync_core.c` | pure snapshot/queue/ACK/retract logic (store + transport vtables) |
| `src/c/sync.c` | AppMessage adapter wiring `sync_core` to `app_message` + `store.c` |
| wiring | `seed.c` now serves the cache; `FX_SAVE_SESSION` enqueues; `main.c` re-snapshots on commit, drains the queue each minute |

Tests: `cd tests/c && ./run.sh` (128 C: digit_entry 16, session 19,
state_machine 53, store_core 23, sync_core 17) and `cd tests/js && ./run.sh`
(29: datastore, library, config page (jsdom), index bridge). jsdom is
installed locally with `npm i --no-save jsdom@25` in `tests/js/`.

End-to-end on the emulator (snapshot in, session out + ACK, retract) is
recorded in `docs/sp2-e2e-notes.md`.

**Gotchas found in SP2:**
- `message_keys.auto.h` is only regenerated on a **clean** build — run
  `pebble clean && pebble build` after changing `package.json` messageKeys
  or `MESSAGE_KEY_*` won't resolve.
- The Pebble app stack is tiny (~2 KB): never put a
  `QueuedSession[STORE_MAX_QUEUE]` (2 KB) on the stack. `store_queue_remove`
  uses a `static` scratch buffer; `sync_core` only ever peeks the head.
- `enableMultiJS: true` in `package.json` is required for
  `require('./library')` in pkjs.
- `"capabilities": ["configurable"]` in `package.json` is required for the
  phone app to show the settings gear — a `showConfiguration` listener
  alone is not enough. Added in 1.0.1.
- `CONFIG_BASE_URL` in `index.js` is `https://magnetz.github.io/timereader-pebble/`;
  GitHub Pages deploy is `.github/workflows/pages.yml` (serves `config-page/`).

**Remaining:** Task 16 (distribution.md + README distribution pointer),
then publish — Rebble store + `.pbw` on GitHub Releases + Pages enablement
on the `magnetz/timereader-pebble` repo.

## How to work

### Environment (already installed on this machine)

```bash
export PATH="$HOME/.local/bin:$PATH"   # pebble-tool lives here — add to your shell rc
pebble --version                       # Pebble Tool v5.0.40
pebble sdk list                        # 4.33.1 (active)
```

### Build + run the watchapp

```bash
cd ~/app/timereader-pebble/watchapp
pebble build
pebble install --emulator basalt
pebble screenshot --no-open /tmp/shot.png   # then open/inspect the PNG
pebble logs                                 # APP_LOG output from the running app
```

### Host logic tests (no SDK, fast)

```bash
cd ~/app/timereader-pebble/tests/c
./run.sh
```

`run.sh` compiles each `test_*.c` with `-Werror` against the host-safe
logic files (`digit_entry.c`, `session.c`, `state_machine.c`). Keep those
three files free of `#include <pebble.h>` — that is what makes them
host-testable. UI files (`ui_*.c`, `main.c`, `store.c`) do include
`pebble.h` and are only built by `pebble build`.

## Gotchas discovered

- **PATH:** `pebble` is at `~/.local/bin/pebble` (uv tool). Every shell
  needs `export PATH="$HOME/.local/bin:$PATH"`.
- **SyntaxWarnings:** `pebble` prints `SyntaxWarning: invalid escape
  sequence` lines from its own libs on every invocation. Harmless noise;
  filter with `grep -v SyntaxWarning`.
- **Emulator "Connection refused":** the first `pebble install
  --emulator basalt` right after boot occasionally fails with
  `[Errno 61] Connection refused`. Just re-run it. `pebble kill` resets a
  stuck emulator.
- **`.pbw` name:** the bundle is `build/watchapp.pbw` (from the project
  dir name), not `timereader.pbw`. The plan text says `timereader.pbw` in
  a couple of places — ignore that, the file is `watchapp.pbw`.
- **`timeout` is not installed** on this machine (no coreutils). Don't
  wrap commands in `timeout`.
- **wscript** is the stock one and globs `src/c/**/*.c` — no need to
  register new C files, just create them. It also references
  `src/pkjs/index.js` for the JS bundle; that path doesn't exist yet and
  the build is fine without it (SP2 adds it).
- **`package.json`** is pinned: UUID `bfdd20e5-1c38-4c97-85ee-486042b64b96`,
  `targetPlatforms: ["basalt"]` only, `messageKeys: []` (SP2 fills this).

## Conventions

- **Branch:** `sp1-watchapp`. Never commit to `main`.
- **Commits:** Conventional Commits (`feat:`, `test:`, `chore:`, `fix:`,
  `docs:`), one per plan task, and end the message with:
  ```
  Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
  Claude-Session: <your session url>
  ```
- **Plan checkboxes:** tick `- [ ]` → `- [x]` as you finish steps in
  `docs/superpowers/plans/2026-08-27-sp1-watchapp-core.md`.
- After Task 14, run the `superpowers:finishing-a-development-branch`
  skill.

## Open design notes for later

- SP1 stubs the "save session" side effect (`FX_SAVE_SESSION` just clears
  the recovery record). SP2 adds the persistent session queue +
  AppMessage. The state machine already emits `FX_SAVE_SESSION` /
  `FX_RETRACT_SESSION` with all the data a queue writer needs
  (`c->entry`, `c->live`, `c->start_page_for_session`, `c->book_index`).
- UI must match system apps: native `MenuLayer` / `ActionMenu` /
  `StatusBarLayer` / `ActionBarLayer`, system window transitions, system
  fonts only, system animation curves. See the spec's "Aderenza alle
  linee guida UI ufficiali Pebble" section — Task 14 audits against it.
