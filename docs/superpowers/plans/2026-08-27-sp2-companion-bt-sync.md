# TimeReader Pebble SP2 — Companion PebbleKit JS + Bluetooth Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the SP1 watchapp real data and persistent reading history: a PebbleKit JS companion (`datastore.js` + `library.js` + `index.js`) bundled in the `.pbw`, a static GitHub-Pages config page to manage the library, and an AppMessage sync protocol (full book snapshot in, completed-session queue out, idempotent ACKs, retract).

**Architecture:** The phone is the offline source of truth. `datastore.js` is the only module that knows persistence (localStorage today, `fetch()` after the future server migration) and hides it behind an async interface. `library.js` computes all per-book statistics so the watch does no arithmetic — it just caches one digested record per book. Sync is stateless full-snapshot on every watchapp launch plus a persistent one-at-a-time session queue with idempotent ACKs. The watch-side sync decision logic lives in a Pebble-header-free `sync_core.c` (tested on the host with fake store/transport), mirroring SP1's `state_machine.c` pattern; `sync.c` is the thin PebbleOS adapter.

**Tech Stack:** JavaScript (PebbleKit JS / CommonJS, `enableMultiJS`), `node:test` for JS unit tests, jsdom for the config page, C (Pebble SDK 4.x, `basalt`) for `sync.c` / `store.c`, host `cc` for `sync_core` / `store_core` tests, `pebble emu-app-config` for the end-to-end config cycle.

**Spec:** `docs/superpowers/specs/2026-08-27-companion-bt-sync-design.md`. Its "Modello dati", "Protocollo di sincronizzazione (AppMessage)", "Companion PebbleKit JS", "Config page" and "Error handling" sections are the SP2 contract. Cross-reference the original webapp at `../m5/timereader/firmware/webapp/` for the config-page behaviour being ported and `../m5/timereader/firmware/app/` for the stats rules.

## Global Constraints

- Target platform: **`basalt` only**. App UUID (verbatim): `bfdd20e5-1c38-4c97-85ee-486042b64b96`.
- Python for `pebble-tool`: **3.13**. `pebble` binary lives at `~/.local/bin/pebble` — every shell needs `export PATH="$HOME/.local/bin:$PATH"`. `pebble` prints harmless `SyntaxWarning` lines; filter with `grep -v SyntaxWarning`.
- The built bundle is `watchapp/build/watchapp.pbw` (not `timereader.pbw` — the spec text is wrong on this point).
- `timeout` is **not** installed on this machine. Never wrap commands in it. `pebble kill` resets a stuck emulator; the first `pebble install --emulator basalt` after boot occasionally fails with `[Errno 61] Connection refused` — just re-run it.
- **Offline-first, server later:** SP2 works completely offline. All persistence access goes through `datastore.js`'s async interface so a future migration replaces only that file's body with `fetch()`. `library.js`, `index.js` and the config page must never touch `localStorage` directly.
- **No sync in background:** AppMessage only moves data while the watchapp is in the foreground and pkjs is alive. All sync happens in the "watchapp open" window.
- **Reading-rate rule (verbatim from the original spec):** average pages/hour is `Σ pages / Σ duration_seconds` converted to hours — **never** the mean of per-session rates. A test asserts this explicitly.
- **Inclusive page counting (SP1 user rule, carried forward):** a session over pages `S..E` covers `E - S + 1` pages. `library.js` validations and any page math must match.
- **current_page rule (verbatim from the original):** a book's `current_page` field has effect only until the book has sessions of its own; after that the current page derives from the last session's `end_page` (computed in `library.js`).
- **Global-estimate rule:** a book with no sessions of its own gets `pages_per_hour` = the cumulative average across all books, with `pph_is_estimate = true`; as soon as it has one real session it switches to its own rate and the flag clears.
- **Page validation:** `end_page >= start_page`; `duration_seconds >= 0`. Invalid sessions are rejected (config page shows an inline error; the watch never produces one — SP1's state machine already blocks `end < start`).
- **Digested book record (watch cache), fields verbatim:** `id, title, color_state, current_page, total_pages, pages_per_hour (×100 int), total_hours (×100 int), flags` where `flags` bit0 = `pph_is_estimate`, bit1 = `favorite`, bit2 = `completed`. `color_state`: `completed` (green) / `started` (cyan) / `unread` (white), computed in pkjs.
- **AppMessage outbox is small (~256 B):** exactly one message in flight; the next `BOOK` / `SESSION` is sent only after `APP_MSG_OK` for the previous one.
- **Atomic snapshot:** the watch accumulates incoming `BOOK` records into a shadow area; only `SNAPSHOT_END` commits shadow → cache. A snapshot interrupted between `SNAPSHOT_BEGIN` and `SNAPSHOT_END` discards the shadow and keeps the previous good cache.
- **Retract is narrow:** only from the Summary screen, only for the just-closed session. Once back at book detail the session is final (editable only from the config page).
- **`PERSIST_DATA_MAX_LENGTH` on basalt is 256 bytes.** The books cache and the session queue are stored one record per numbered persist key, never one big blob.
- All commits use Conventional Commits (`feat:`, `test:`, `chore:`, `fix:`, `docs:`), one per task. End every commit message with:
  ```
  Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
  Claude-Session: <session url>
  ```
- Branch: create `sp2-companion` off `main` before Task 1. Never commit to `main`.
- Tick plan checkboxes `- [ ]` → `- [x]` as steps complete.

---

## File Structure

```
timereader-pebble/
  watchapp/
    package.json              # + messageKeys (19 keys), enableMultiJS, resources unchanged
    src/pkjs/
      index.js                # lifecycle: ready -> snapshot; appmessage -> SESSION/RETRACT; showConfiguration/webviewclosed
      library.js              # pure: stats, colorState, digestBook, computeSnapshot, validateSession
      datastore.js            # SEAM: async CRUD over localStorage (keys tr_books / tr_sessions); _setStorage() for tests
    src/c/
      model.h                 # (SP1) + QueuedSession struct
      store.h / store.c       # (SP1) + books cache (numbered keys) + shadow + session queue + session-id counter
      store_core.h / store_core.c   # NEW: pure serialise/deserialise + queue/shadow index math (no pebble.h)
      sync.h / sync.c         # NEW: PebbleOS AppMessage adapter -> sync_core
      sync_core.h / sync_core.c     # NEW: pure snapshot/queue/ACK/retract decision logic (no pebble.h)
      seed.h / seed.c         # CHANGED: seed_books() now serves the store cache (empty => APP_NO_BOOKS)
      ui_common.c             # CHANGED: FX_SAVE_SESSION / FX_RETRACT_SESSION now feed the sync queue
      main.c                  # CHANGED: sync_init(); snapshot-commit -> rebuild books + refresh; launch/tick queue drain
  config-page/
    index.html               # single file, inline CSS/JS, no deps; hash in -> pebblejs://close out
  tests/
    c/
      test_store_core.c       # NEW
      test_sync_core.c        # NEW
      fake_sync_env.h         # NEW: in-memory store + transport fakes for sync_core tests
    js/
      run.sh                  # NEW: node --test
      test_datastore.mjs      # NEW
      test_library.mjs        # NEW
      test_config_page.mjs    # NEW (jsdom)
      test_index_bridge.mjs   # NEW (datastore diff on webviewclosed)
  docs/
    on-device-checklist.md    # + SP2 section
    HANDOFF.md                # updated
```

`library.js` and `datastore.js` are hardware-agnostic CommonJS modules (`module.exports = { ... }`), loaded by `index.js` with `require('./library')` / `require('./datastore')` (needs `enableMultiJS: true`) and by the Node tests the same way. `sync_core.c` and `store_core.c` include **only** `model.h` / `store_core.h` and standard C headers so `tests/c/run.sh` compiles them with `cc`. `sync.c` and `store.c` include `<pebble.h>` and are built only by `pebble build`.

`tests/c/run.sh` (from SP1) auto-includes `digit_entry.c session.c state_machine.c`; extend its loop list to also try `store_core.c sync_core.c`.

---

## Task 1: Branch + JS test harness + `datastore.js`

Port of the persistence seam. `datastore.js` is the only module that knows storage. Async (returns Promises) so the future server migration swaps only its body.

**Files:**
- Create: `watchapp/src/pkjs/datastore.js`, `tests/js/run.sh`, `tests/js/test_datastore.mjs`

**Interfaces:**
- Consumes: nothing (a storage object; defaults to `localStorage`, overridable via `_setStorage`).
- Produces (`module.exports`):
  ```js
  // all return Promises
  getBooks()                 // -> Book[] sorted by .order asc
  saveBook(book)             // upsert by id; if no id, assign id "b_"+6hex and order = max(order)+1; returns the stored book
  deleteBook(id)             // also deletes that book's sessions
  reorderBooks(orderedIds)   // sets .order to the index in orderedIds
  getSessions(bookId)        // bookId optional; -> Session[] (all, or for one book) sorted by created_at asc
  appendSession(session)     // idempotent by .id: if id already present, returns existing without duplicating; else assigns created_at = Date.now()/1000 |0 and stores
  updateSession(session)     // replace by .id; throws if absent
  deleteSession(id)          // no-op if absent
  _setStorage(impl)          // test hook: impl = { getItem(k), setItem(k,v) }
  _genId(prefix)             // exported for reuse by index.js session ids
  ```
  `Book`: `{ id, title, series, author, total_pages, current_page, favorite, order, created_at }`.
  `Session`: `{ id, book_id, start_page, end_page, pages, duration_seconds, source, created_at }`.
  Storage keys: `tr_books`, `tr_sessions`, each a JSON array string.

- [x] **Step 1: Branch**

```bash
cd /Users/lucamagnetti/app/timereader-pebble
git checkout main && git pull --ff-only 2>/dev/null; git checkout -b sp2-companion
```

- [x] **Step 2: Write `tests/js/run.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
node --test
```
`chmod +x tests/js/run.sh`. (Node's built-in runner picks up `test_*.mjs` in the cwd.)

- [x] **Step 3: Write the failing tests** — `tests/js/test_datastore.mjs`

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import ds from '../../watchapp/src/pkjs/datastore.js';

function fakeStorage() {
  const m = new Map();
  return { getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)) };
}

test('saveBook assigns id and order, getBooks sorts by order', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A', total_pages: 100 });
  const b = await ds.saveBook({ title: 'B', total_pages: 200 });
  assert.match(a.id, /^b_[0-9a-f]{6}$/);
  assert.equal(a.order, 1);
  assert.equal(b.order, 2);
  const books = await ds.getBooks();
  assert.deepEqual(books.map((x) => x.title), ['A', 'B']);
});

test('saveBook upserts by id', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  await ds.saveBook({ ...a, title: 'A2' });
  const books = await ds.getBooks();
  assert.equal(books.length, 1);
  assert.equal(books[0].title, 'A2');
});

test('appendSession is idempotent by id', async () => {
  ds._setStorage(fakeStorage());
  const s = { id: 's_x', book_id: 'b_1', start_page: 1, end_page: 10, pages: 10, duration_seconds: 60, source: 'watch' };
  await ds.appendSession(s);
  await ds.appendSession(s);
  const all = await ds.getSessions();
  assert.equal(all.length, 1);
  assert.ok(all[0].created_at > 0);
});

test('deleteBook cascades to its sessions', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  await ds.appendSession({ id: 's1', book_id: a.id, start_page: 1, end_page: 5, pages: 5, duration_seconds: 60, source: 'manual' });
  await ds.appendSession({ id: 's2', book_id: b.id, start_page: 1, end_page: 5, pages: 5, duration_seconds: 60, source: 'manual' });
  await ds.deleteBook(a.id);
  assert.deepEqual((await ds.getBooks()).map((x) => x.id), [b.id]);
  assert.deepEqual((await ds.getSessions()).map((x) => x.id), ['s2']);
});

test('reorderBooks rewrites .order to index position', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  const c = await ds.saveBook({ title: 'C' });
  await ds.reorderBooks([c.id, a.id, b.id]);
  assert.deepEqual((await ds.getBooks()).map((x) => x.title), ['C', 'A', 'B']);
});

test('updateSession throws when the id is absent', async () => {
  ds._setStorage(fakeStorage());
  await assert.rejects(() => ds.updateSession({ id: 'nope' }));
});
```

- [x] **Step 4: Run to verify it fails**

Run: `cd tests/js && ./run.sh`
Expected: cannot find module `datastore.js` / assertion failures.

- [x] **Step 5: Write `datastore.js`**

```js
var _storage = (typeof localStorage !== 'undefined') ? localStorage : null;

function _setStorage(impl) { _storage = impl; }
function _read(key) { try { return JSON.parse(_storage.getItem(key)) || []; } catch (e) { return []; } }
function _write(key, arr) { _storage.setItem(key, JSON.stringify(arr)); }
function _genId(prefix) {
  var s = '';
  for (var i = 0; i < 6; i++) s += Math.floor(Math.random() * 16).toString(16);
  return prefix + s;
}

function getBooks() {
  return Promise.resolve(_read('tr_books').slice().sort(function (a, b) { return a.order - b.order; }));
}
function saveBook(book) {
  var books = _read('tr_books');
  var b = Object.assign({}, book);
  if (!b.id) {
    b.id = _genId('b_');
    b.order = books.reduce(function (m, x) { return Math.max(m, x.order || 0); }, 0) + 1;
    b.created_at = Math.floor(Date.now() / 1000);
    b.favorite = !!b.favorite;
  }
  var i = books.findIndex(function (x) { return x.id === b.id; });
  if (i >= 0) books[i] = Object.assign({}, books[i], b); else books.push(b);
  _write('tr_books', books);
  return Promise.resolve(b);
}
function deleteBook(id) {
  _write('tr_books', _read('tr_books').filter(function (x) { return x.id !== id; }));
  _write('tr_sessions', _read('tr_sessions').filter(function (s) { return s.book_id !== id; }));
  return Promise.resolve();
}
function reorderBooks(orderedIds) {
  var books = _read('tr_books');
  books.forEach(function (b) {
    var idx = orderedIds.indexOf(b.id);
    if (idx >= 0) b.order = idx + 1;
  });
  _write('tr_books', books);
  return Promise.resolve();
}
function getSessions(bookId) {
  var all = _read('tr_sessions').slice().sort(function (a, b) { return a.created_at - b.created_at; });
  return Promise.resolve(bookId ? all.filter(function (s) { return s.book_id === bookId; }) : all);
}
function appendSession(session) {
  var sessions = _read('tr_sessions');
  var existing = sessions.find(function (s) { return s.id === session.id; });
  if (existing) return Promise.resolve(existing);
  var s = Object.assign({ source: 'watch' }, session, { created_at: Math.floor(Date.now() / 1000) });
  sessions.push(s);
  _write('tr_sessions', sessions);
  return Promise.resolve(s);
}
function updateSession(session) {
  var sessions = _read('tr_sessions');
  var i = sessions.findIndex(function (s) { return s.id === session.id; });
  if (i < 0) return Promise.reject(new Error('no such session ' + session.id));
  sessions[i] = Object.assign({}, sessions[i], session);
  _write('tr_sessions', sessions);
  return Promise.resolve(sessions[i]);
}
function deleteSession(id) {
  _write('tr_sessions', _read('tr_sessions').filter(function (s) { return s.id !== id; }));
  return Promise.resolve();
}

module.exports = {
  getBooks: getBooks, saveBook: saveBook, deleteBook: deleteBook, reorderBooks: reorderBooks,
  getSessions: getSessions, appendSession: appendSession, updateSession: updateSession, deleteSession: deleteSession,
  _setStorage: _setStorage, _genId: _genId,
};
```

- [x] **Step 6: Run to verify it passes**

Run: `cd tests/js && ./run.sh`
Expected: all datastore tests pass.

- [x] **Step 7: Commit**

```bash
git add watchapp/src/pkjs/datastore.js tests/js/
git commit -m "feat: datastore.js persistence seam with node tests"
```

---

## Task 2: `library.js` — core stats + colour + validation

Port of `session.py` / `completion_estimate.py` / `storage.py` rules to JS. Pure functions, no storage.

**Files:**
- Create: `watchapp/src/pkjs/library.js`, `tests/js/test_library.mjs`

**Interfaces:**
- Consumes: nothing (takes plain book / session objects).
- Produces (`module.exports`):
  ```js
  pagesPerHour(sessions)                 // Σpages / (Σduration_seconds/3600); 0 if Σduration == 0
  totalHours(sessions)                   // Σduration_seconds / 3600
  bookCurrentPage(book, sessions)        // sessions.length ? last-by-created_at .end_page : (book.current_page || 0)
  globalPagesPerHour(allSessions)        // pagesPerHour over every session, all books
  colorState(book, sessions)             // "completed" if bookCurrentPage >= total_pages (>0); "started" if sessions.length || current_page>0; else "unread"
  validateSession(s)                     // returns null, or an error string: "end_page < start_page" / "duration_seconds < 0"
  ```

- [x] **Step 1: Write the failing tests** — `tests/js/test_library.mjs`

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import lib from '../../watchapp/src/pkjs/library.js';

test('pagesPerHour is sum/sum, not the mean of per-session rates', () => {
  // A: 90 pages / 1h, B: 10 pages / 3h -> 100 / 4h = 25.0
  // mean of per-session rates would be (90 + 3.33)/2 = 46.67
  const ss = [
    { pages: 90, duration_seconds: 3600 },
    { pages: 10, duration_seconds: 10800 },
  ];
  assert.equal(Math.round(lib.pagesPerHour(ss) * 100), 2500);
  assert.equal(lib.pagesPerHour([]), 0);
});

test('bookCurrentPage prefers the last session end_page', () => {
  const book = { current_page: 5, total_pages: 100 };
  assert.equal(lib.bookCurrentPage(book, []), 5);
  assert.equal(lib.bookCurrentPage(book, [
    { end_page: 40, created_at: 1 }, { end_page: 73, created_at: 2 },
  ]), 73);
});

test('colorState', () => {
  assert.equal(lib.colorState({ current_page: 0, total_pages: 300 }, []), 'unread');
  assert.equal(lib.colorState({ current_page: 12, total_pages: 300 }, []), 'started');
  assert.equal(lib.colorState({ current_page: 0, total_pages: 300 },
    [{ end_page: 40, created_at: 1 }]), 'started');
  assert.equal(lib.colorState({ current_page: 0, total_pages: 300 },
    [{ end_page: 300, created_at: 1 }]), 'completed');
});

test('validateSession', () => {
  assert.equal(lib.validateSession({ start_page: 10, end_page: 10, duration_seconds: 0 }), null);
  assert.equal(lib.validateSession({ start_page: 10, end_page: 9, duration_seconds: 5 }), 'end_page < start_page');
  assert.equal(lib.validateSession({ start_page: 1, end_page: 5, duration_seconds: -1 }), 'duration_seconds < 0');
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd tests/js && ./run.sh` — module missing.

- [ ] **Step 3: Write `library.js` (this task's exports only)**

```js
function _sum(a, f) { return a.reduce(function (m, x) { return m + f(x); }, 0); }
function _lastBy(a, key) {
  return a.slice().sort(function (x, y) { return (x[key] || 0) - (y[key] || 0); }).pop();
}

function pagesPerHour(sessions) {
  var secs = _sum(sessions, function (s) { return s.duration_seconds || 0; });
  if (secs <= 0) return 0;
  return _sum(sessions, function (s) { return s.pages || 0; }) / (secs / 3600);
}
function totalHours(sessions) {
  return _sum(sessions, function (s) { return s.duration_seconds || 0; }) / 3600;
}
function bookCurrentPage(book, sessions) {
  if (sessions && sessions.length) return _lastBy(sessions, 'created_at').end_page;
  return book.current_page || 0;
}
function globalPagesPerHour(allSessions) { return pagesPerHour(allSessions || []); }
function colorState(book, sessions) {
  var cur = bookCurrentPage(book, sessions || []);
  if (book.total_pages > 0 && cur >= book.total_pages) return 'completed';
  if ((sessions && sessions.length) || (book.current_page || 0) > 0) return 'started';
  return 'unread';
}
function validateSession(s) {
  if (s.end_page < s.start_page) return 'end_page < start_page';
  if (s.duration_seconds < 0) return 'duration_seconds < 0';
  return null;
}

module.exports = {
  pagesPerHour: pagesPerHour, totalHours: totalHours, bookCurrentPage: bookCurrentPage,
  globalPagesPerHour: globalPagesPerHour, colorState: colorState, validateSession: validateSession,
};
```

- [ ] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [ ] **Step 5: Commit**

```bash
git add watchapp/src/pkjs/library.js tests/js/test_library.mjs
git commit -m "feat: library.js reading-rate, current-page and colour rules"
```

---

## Task 3: `library.js` — `bookStats`, global estimate, `digestBook`, `computeSnapshot`

Adds the estimate fallback and the digested-record builder the watch consumes.

**Files:**
- Modify: `watchapp/src/pkjs/library.js`
- Test: `tests/js/test_library.mjs` (append)

**Interfaces:**
- Consumes: Task 2 exports.
- Produces (added to `module.exports`):
  ```js
  bookStats(book, sessions, globalPph)
    // -> { totalHours, pagesPerHour, currentPage, pagesLeft, etaMinutes, pphIsEstimate }
    //    pphIsEstimate = sessions.length === 0; pagesPerHour = estimate ? globalPph : pagesPerHour(sessions)
    //    pagesLeft = max(0, total_pages - currentPage); etaMinutes = pagesPerHour>0 ? pagesLeft / pagesPerHour * 60 : 0
  digestBook(book, sessions, globalPph)
    // -> { id, title, color, cur_page, tot_pages, pph_x100, hours_x100, flags }
    //    color: 0 unread / 1 started / 2 completed
    //    pph_x100 = round(stats.pagesPerHour*100); hours_x100 = round(stats.totalHours*100)
    //    flags = (pphIsEstimate?1:0) | (book.favorite?2:0) | (color===2?4:0)
  computeSnapshot(books, sessionsByBook)
    // books sorted by order; sessionsByBook: { [bookId]: Session[] }
    // globalPph computed once from every session; -> digestBook[] in book order
  ```

- [x] **Step 1: Append failing tests**

```js
test('bookStats falls back to the global rate until the book has a session', () => {
  const book = { total_pages: 200, current_page: 20, favorite: false };
  const withNone = lib.bookStats(book, [], 30);
  assert.equal(withNone.pphIsEstimate, true);
  assert.equal(withNone.pagesPerHour, 30);
  assert.equal(withNone.currentPage, 20);
  assert.equal(withNone.pagesLeft, 180);
  assert.equal(Math.round(withNone.etaMinutes), 360);

  const withOne = lib.bookStats(book, [{ pages: 40, duration_seconds: 3600, end_page: 60, created_at: 1 }], 30);
  assert.equal(withOne.pphIsEstimate, false);
  assert.equal(withOne.pagesPerHour, 40);
  assert.equal(withOne.currentPage, 60);
});

test('digestBook packs flags and x100 ints', () => {
  const d = lib.digestBook(
    { id: 'b1', title: 'T', total_pages: 100, current_page: 0, favorite: true },
    [{ pages: 50, duration_seconds: 3600, end_page: 100, created_at: 1 }], 10);
  assert.equal(d.color, 2);           // completed
  assert.equal(d.pph_x100, 5000);
  assert.equal(d.hours_x100, 100);
  assert.equal(d.flags, 0 | 2 | 4);   // not estimate, favorite, completed
});

test('computeSnapshot returns records in book order with one global rate', () => {
  const books = [
    { id: 'b2', title: 'Two', order: 2, total_pages: 100, current_page: 0, favorite: false },
    { id: 'b1', title: 'One', order: 1, total_pages: 100, current_page: 0, favorite: false },
  ];
  const snap = lib.computeSnapshot(books, {
    b1: [{ pages: 20, duration_seconds: 3600, end_page: 20, created_at: 1 }],
    b2: [],
  });
  assert.deepEqual(snap.map((d) => d.id), ['b1', 'b2']);
  assert.equal(snap[1].flags & 1, 1); // b2 has no sessions -> estimate flag
});
```

- [x] **Step 2: Run to verify it fails** — `cd tests/js && ./run.sh`.

- [x] **Step 3: Implement in `library.js`**

```js
function bookStats(book, sessions, globalPph) {
  sessions = sessions || [];
  var estimate = sessions.length === 0;
  var pph = estimate ? (globalPph || 0) : pagesPerHour(sessions);
  var cur = bookCurrentPage(book, sessions);
  var left = Math.max(0, (book.total_pages || 0) - cur);
  return {
    totalHours: totalHours(sessions),
    pagesPerHour: pph,
    currentPage: cur,
    pagesLeft: left,
    etaMinutes: pph > 0 ? (left / pph) * 60 : 0,
    pphIsEstimate: estimate,
  };
}
function _colorCode(name) { return name === 'completed' ? 2 : name === 'started' ? 1 : 0; }
function digestBook(book, sessions, globalPph) {
  var st = bookStats(book, sessions, globalPph);
  var color = _colorCode(colorState(book, sessions));
  var flags = (st.pphIsEstimate ? 1 : 0) | (book.favorite ? 2 : 0) | (color === 2 ? 4 : 0);
  return {
    id: book.id, title: book.title || '',
    color: color, cur_page: st.currentPage, tot_pages: book.total_pages || 0,
    pph_x100: Math.round(st.pagesPerHour * 100), hours_x100: Math.round(st.totalHours * 100),
    flags: flags,
  };
}
function computeSnapshot(books, sessionsByBook) {
  sessionsByBook = sessionsByBook || {};
  var every = [];
  Object.keys(sessionsByBook).forEach(function (k) { every = every.concat(sessionsByBook[k] || []); });
  var g = globalPagesPerHour(every);
  return books.slice().sort(function (a, b) { return (a.order || 0) - (b.order || 0); })
    .map(function (b) { return digestBook(b, sessionsByBook[b.id] || [], g); });
}
```
Add `bookStats, digestBook, computeSnapshot` to `module.exports`.

- [x] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [x] **Step 5: Commit**

```bash
git add watchapp/src/pkjs/library.js tests/js/test_library.mjs
git commit -m "feat: library.js book stats, global estimate and digest snapshot"
```

---

## Task 4: Config page — skeleton, hash decode, book list, HTML escaping

Single static `index.html`. Stateless: it renders whatever the URL hash carries and returns a result on save. No external requests, theme-agnostic (define both light and dark palettes with plain CSS custom properties; no framework).

**Files:**
- Create: `config-page/index.html`, `tests/js/test_config_page.mjs`

**Interfaces:**
- Consumes: URL hash = `#` + `encodeURIComponent(JSON.stringify({ books, sessionsByBook, globalPph }))`.
- Produces: on "Salva", `document.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result))`, `result = { books: Book[], sessionOps: Array<{op:'add'|'update'|'delete', session?, id?}> }`.
- For tests, the page exposes a `window.__tr` object: `{ decode(hash), render(state), escapeHtml(s), buildResult() }`.

- [x] **Step 1: Write the failing tests** — `tests/js/test_config_page.mjs`

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { JSDOM } from 'jsdom';

const HTML = readFileSync(new URL('../../config-page/index.html', import.meta.url), 'utf8');

function load(state) {
  const hash = '#' + encodeURIComponent(JSON.stringify(state));
  const dom = new JSDOM(HTML, { url: 'https://example.org/' + hash, runScripts: 'dangerously' });
  return dom.window;
}

test('renders one row per book with its colour state', () => {
  const w = load({
    books: [
      { id: 'b1', title: 'Alpha', total_pages: 100, current_page: 100, favorite: false, order: 1 },
      { id: 'b2', title: 'Beta', total_pages: 100, current_page: 0, favorite: false, order: 2 },
    ],
    sessionsByBook: { b1: [], b2: [] }, globalPph: 20,
  });
  const rows = w.document.querySelectorAll('[data-book-row]');
  assert.equal(rows.length, 2);
  assert.equal(rows[0].getAttribute('data-color'), 'completed');
  assert.equal(rows[1].getAttribute('data-color'), 'unread');
});

test('escapeHtml neutralises < " \' in titles', () => {
  const w = load({ books: [], sessionsByBook: {}, globalPph: 0 });
  assert.equal(w.__tr.escapeHtml(`<img src=x> "q" 'a'`), '&lt;img src=x&gt; &quot;q&quot; &#39;a&#39;');
});

test('a title with markup is shown as text, not parsed', () => {
  const w = load({
    books: [{ id: 'b1', title: '<b>x</b>', total_pages: 10, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0,
  });
  const row = w.document.querySelector('[data-book-row]');
  assert.equal(row.querySelector('b'), null);
  assert.match(row.textContent, /<b>x<\/b>/);
});
```

- [x] **Step 2: Run to verify it fails**

Run: `cd tests/js && npm i --no-save jsdom >/dev/null 2>&1; ./run.sh`
(Install `jsdom` once into `tests/js/node_modules`; add `tests/js/node_modules/` and `tests/js/package.json` to `.gitignore` — pin the version in a comment in `run.sh`.)
Expected: file missing / assertion failures.

- [x] **Step 3: Write `config-page/index.html` (this task's scope)**

A single file. `<head>`: `<meta name="viewport">`, inline `<style>` with `:root` light tokens and `@media (prefers-color-scheme: dark)` overrides, list/row/button classes. `<body>`: `<h1>TimeReader</h1>`, `<div id="book-list"></div>`, `<button id="save">Salva</button>`, `<button id="cancel">Annulla</button>`, then one inline `<script>`:

```html
<script>
(function () {
  var state = { books: [], sessionsByBook: {}, globalPph: 0 };
  var ops = [];                       // session ops accumulated this session

  function decode(hash) {
    try { return JSON.parse(decodeURIComponent((hash || '').replace(/^#/, ''))); }
    catch (e) { return { books: [], sessionsByBook: {}, globalPph: 0 }; }
  }
  function escapeHtml(s) {
    return String(s).replace(/[<>&"']/g, function (c) {
      return { '<': '&lt;', '>': '&gt;', '&': '&amp;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }
  function currentPage(book, sessions) {
    if (sessions && sessions.length) {
      return sessions.slice().sort(function (a, b) { return a.created_at - b.created_at; }).pop().end_page;
    }
    return book.current_page || 0;
  }
  function colorState(book, sessions) {
    var cur = currentPage(book, sessions);
    if (book.total_pages > 0 && cur >= book.total_pages) return 'completed';
    if ((sessions && sessions.length) || (book.current_page || 0) > 0) return 'started';
    return 'unread';
  }
  function renderBooks() {
    var host = document.getElementById('book-list');
    host.innerHTML = '';
    state.books.slice().sort(function (a, b) { return a.order - b.order; }).forEach(function (b) {
      var sessions = state.sessionsByBook[b.id] || [];
      var row = document.createElement('div');
      row.setAttribute('data-book-row', '');
      row.setAttribute('data-book-id', b.id);
      row.setAttribute('data-color', colorState(b, sessions));
      row.innerHTML = '<span class="title">' + escapeHtml(b.title) + '</span>' +
        '<span class="meta">' + currentPage(b, sessions) + '/' + (b.total_pages || '?') + '</span>';
      host.appendChild(row);
    });
  }
  function render(s) { state = s; renderBooks(); }
  function buildResult() { return { books: state.books, sessionOps: ops }; }

  window.__tr = { decode: decode, render: render, escapeHtml: escapeHtml, buildResult: buildResult,
                  _state: function () { return state; }, _ops: function () { return ops; } };

  document.addEventListener('DOMContentLoaded', function () {
    render(decode(location.hash));
    document.getElementById('save').addEventListener('click', function () {
      location.href = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(buildResult()));
    });
    document.getElementById('cancel').addEventListener('click', function () {
      location.href = 'pebblejs://close#' + encodeURIComponent(JSON.stringify({ cancelled: true }));
    });
  });
})();
</script>
```

- [x] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [x] **Step 5: Commit**

```bash
git add config-page/index.html tests/js/test_config_page.mjs .gitignore tests/js/run.sh
git commit -m "feat: config page skeleton with hash decode and HTML escaping"
```

---

## Task 5: Config page — book create/edit form, delete, favorite, reorder

**Files:**
- Modify: `config-page/index.html`
- Test: `tests/js/test_config_page.mjs` (append)

**Interfaces:**
- Produces: `window.__tr` gains `openForm(bookId?)`, `submitForm(fields)`, `deleteBook(id)`, `toggleFavorite(id)`, `move(id, dir)` (`dir` = -1 up / +1 down). All mutate `state.books` in place; favourites always sort above non-favourites, ties broken by `order`.

- [x] **Step 1: Append failing tests**

```js
test('submitForm adds a new book at the end', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 10, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0 });
  w.__tr.openForm();
  w.__tr.submitForm({ title: 'New', series: '', author: '', total_pages: 50, current_page: 3 });
  const books = w.__tr._state().books;
  assert.equal(books.length, 2);
  assert.equal(books[1].title, 'New');
  assert.equal(books[1].order, 2);
});

test('toggleFavorite pins the book above non-favourites', () => {
  const w = load({ books: [
    { id: 'b1', title: 'A', total_pages: 10, current_page: 0, favorite: false, order: 1 },
    { id: 'b2', title: 'B', total_pages: 10, current_page: 0, favorite: false, order: 2 },
  ], sessionsByBook: { b1: [], b2: [] }, globalPph: 0 });
  w.__tr.toggleFavorite('b2');
  const rows = w.document.querySelectorAll('[data-book-row]');
  assert.equal(rows[0].getAttribute('data-book-id'), 'b2');
});

test('deleteBook queues a delete for its sessions too', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 10, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [{ id: 's1', book_id: 'b1', start_page: 1, end_page: 5, created_at: 1 }] }, globalPph: 0 });
  w.__tr.deleteBook('b1');
  assert.equal(w.__tr._state().books.length, 0);
  assert.deepEqual(w.__tr._ops().filter((o) => o.op === 'delete').map((o) => o.id), ['s1']);
});
```

- [x] **Step 2: Run to verify it fails** — `cd tests/js && ./run.sh`.

- [x] **Step 3: Implement in `index.html`**

Add a hidden `<form id="book-form">` (title / series / author / total_pages / current_page inputs, submit label toggles "+ Aggiungi libro" ⇄ "Salva modifiche", plus "Annulla"). Row actions (edit / delete / ★ / ▲ / ▼ buttons) rendered per row. In the script:

```js
  var editingId = null;
  function nextOrder() { return state.books.reduce(function (m, b) { return Math.max(m, b.order || 0); }, 0) + 1; }
  function sortedBooks() {
    return state.books.slice().sort(function (a, b) {
      if (!!a.favorite !== !!b.favorite) return a.favorite ? -1 : 1;
      return (a.order || 0) - (b.order || 0);
    });
  }
  function openForm(bookId) {
    editingId = bookId || null;
    var b = editingId ? state.books.find(function (x) { return x.id === editingId; }) : null;
    var f = document.getElementById('book-form');
    f.title.value = b ? b.title : ''; f.series.value = b ? (b.series || '') : '';
    f.author.value = b ? (b.author || '') : '';
    f.total_pages.value = b ? b.total_pages : ''; f.current_page.value = b ? (b.current_page || 0) : 0;
    f.querySelector('[type=submit]').textContent = editingId ? 'Salva modifiche' : '+ Aggiungi libro';
    f.hidden = false;
  }
  function submitForm(fields) {
    if (editingId) {
      var b = state.books.find(function (x) { return x.id === editingId; });
      Object.assign(b, {
        title: fields.title, series: fields.series, author: fields.author,
        total_pages: +fields.total_pages || 0, current_page: +fields.current_page || 0,
      });
    } else {
      state.books.push({
        id: 'b_' + Math.random().toString(16).slice(2, 8), title: fields.title,
        series: fields.series, author: fields.author, total_pages: +fields.total_pages || 0,
        current_page: +fields.current_page || 0, favorite: false, order: nextOrder(),
        created_at: Math.floor(Date.now() / 1000),
      });
    }
    editingId = null;
    document.getElementById('book-form').hidden = true;
    renderBooks();
  }
  function deleteBook(id) {
    (state.sessionsByBook[id] || []).forEach(function (s) { ops.push({ op: 'delete', id: s.id }); });
    state.books = state.books.filter(function (b) { return b.id !== id; });
    delete state.sessionsByBook[id];
    renderBooks();
  }
  function toggleFavorite(id) {
    var b = state.books.find(function (x) { return x.id === id; });
    b.favorite = !b.favorite;
    renderBooks();
  }
  function move(id, dir) {
    var ordered = sortedBooks();
    var i = ordered.findIndex(function (b) { return b.id === id; });
    var j = i + dir;
    if (j < 0 || j >= ordered.length) return;
    var a = ordered[i], b = ordered[j];
    var t = a.order; a.order = b.order; b.order = t;
    if (!!a.favorite !== !!b.favorite) a.favorite = b.favorite; // keep the swap meaningful across the fav boundary
    renderBooks();
  }
```
Extend `renderBooks` to draw the action buttons and wire their listeners; extend `window.__tr` with `openForm, submitForm, deleteBook, toggleFavorite, move`. `buildResult` already returns `state.books`.

- [x] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [x] **Step 5: Commit**

```bash
git add config-page/index.html tests/js/test_config_page.mjs
git commit -m "feat: config page book form, delete, favorite and reorder"
```

---

## Task 6: Config page — per-book sessions tab

**Files:**
- Modify: `config-page/index.html`
- Test: `tests/js/test_config_page.mjs` (append)

**Interfaces:**
- Produces: `window.__tr` gains `openSessions(bookId)`, `addSession(bookId, fields)`, `editSession(id, fields)`, `removeSession(id)`. Each records an entry in `ops` (`add` with a fresh `id` / `update` / `delete`) **and** mirrors the change into `state.sessionsByBook` so the list re-renders and colour states update. Client-side validation reuses the same rules as `library.js` (`end_page >= start_page`, `duration_seconds >= 0`); an invalid submit shows an inline message and records nothing.

- [x] **Step 1: Append failing tests**

```js
test('addSession appends an op and updates the book colour', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 50, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0 });
  w.__tr.openSessions('b1');
  w.__tr.addSession('b1', { start_page: 1, end_page: 50, duration_seconds: 3600 });
  const add = w.__tr._ops().find((o) => o.op === 'add');
  assert.equal(add.session.book_id, 'b1');
  assert.equal(add.session.pages, 50);          // inclusive: 50 - 1 + 1
  assert.match(add.session.id, /^s_/);
  const row = w.document.querySelector('[data-book-row][data-book-id="b1"]');
  assert.equal(row.getAttribute('data-color'), 'completed');
});

test('addSession rejects end < start with an inline error and no op', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 50, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0 });
  w.__tr.openSessions('b1');
  w.__tr.addSession('b1', { start_page: 20, end_page: 5, duration_seconds: 60 });
  assert.equal(w.__tr._ops().length, 0);
  assert.match(w.document.querySelector('#session-error').textContent, /iniziale/i);
});

test('removeSession records a delete and drops it from the list', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 50, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [{ id: 's1', book_id: 'b1', start_page: 1, end_page: 10, pages: 10, duration_seconds: 60, created_at: 1 }] },
    globalPph: 0 });
  w.__tr.openSessions('b1');
  w.__tr.removeSession('s1');
  assert.deepEqual(w.__tr._ops(), [{ op: 'delete', id: 's1' }]);
  assert.equal((w.__tr._state().sessionsByBook.b1 || []).length, 0);
});
```

- [x] **Step 2: Run to verify it fails** — `cd tests/js && ./run.sh`.

- [x] **Step 3: Implement in `index.html`**

Add a `<div id="sessions-panel" hidden>` with a list container, an "+ Aggiungi sessione" form (start_page / end_page / duration in minutes → seconds), an `<p id="session-error">`, and per-row Modifica / Elimina. Script:

```js
  var sessionsBookId = null;
  function validateSession(s) {
    if (s.end_page < s.start_page) return 'La pagina finale è minore di quella iniziale';
    if (s.duration_seconds < 0) return 'La durata non può essere negativa';
    return null;
  }
  function openSessions(bookId) {
    sessionsBookId = bookId;
    document.getElementById('session-error').textContent = '';
    renderSessions();
    document.getElementById('sessions-panel').hidden = false;
  }
  function renderSessions() {
    var host = document.querySelector('#sessions-panel .list');
    host.innerHTML = '';
    (state.sessionsByBook[sessionsBookId] || []).forEach(function (s) {
      var r = document.createElement('div');
      r.setAttribute('data-session-row', s.id);
      r.textContent = 'p.' + s.start_page + '–' + s.end_page + '  ' + Math.round(s.duration_seconds / 60) + ' min';
      host.appendChild(r);
    });
  }
  function _mirror(bookId) { state.sessionsByBook[bookId] = state.sessionsByBook[bookId] || []; return state.sessionsByBook[bookId]; }
  function addSession(bookId, fields) {
    var s = {
      id: 's_' + Math.random().toString(16).slice(2, 8), book_id: bookId,
      start_page: +fields.start_page || 0, end_page: +fields.end_page || 0,
      duration_seconds: +fields.duration_seconds || 0, source: 'manual',
      created_at: Math.floor(Date.now() / 1000),
    };
    s.pages = s.end_page - s.start_page + 1;
    var err = validateSession(s);
    if (err) { document.getElementById('session-error').textContent = err; return; }
    document.getElementById('session-error').textContent = '';
    ops.push({ op: 'add', session: s });
    _mirror(bookId).push(s);
    renderSessions(); renderBooks();
  }
  function editSession(id, fields) {
    var list = _mirror(sessionsBookId);
    var s = list.find(function (x) { return x.id === id; });
    var next = Object.assign({}, s, {
      start_page: +fields.start_page, end_page: +fields.end_page,
      duration_seconds: +fields.duration_seconds,
    });
    next.pages = next.end_page - next.start_page + 1;
    var err = validateSession(next);
    if (err) { document.getElementById('session-error').textContent = err; return; }
    Object.assign(s, next);
    ops.push({ op: 'update', session: s });
    renderSessions(); renderBooks();
  }
  function removeSession(id) {
    ops.push({ op: 'delete', id: id });
    state.sessionsByBook[sessionsBookId] = _mirror(sessionsBookId).filter(function (x) { return x.id !== id; });
    renderSessions(); renderBooks();
  }
```
Extend `window.__tr` with `openSessions, addSession, editSession, removeSession`.

- [x] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [x] **Step 5: Commit**

```bash
git add config-page/index.html tests/js/test_config_page.mjs
git commit -m "feat: config page per-book sessions with client validation"
```

---

## Task 7: `index.js` — config bridge (`showConfiguration` / `webviewclosed` diff)

**Files:**
- Create: `watchapp/src/pkjs/index.js`
- Test: `tests/js/test_index_bridge.mjs`

**Interfaces:**
- Consumes: `datastore.js`, `library.js`.
- Produces (`module.exports` for tests; the real file also registers `Pebble` listeners):
  ```js
  buildConfigPayload()          // -> Promise<{ books, sessionsByBook, globalPph }>
  applyConfigResult(result)     // -> Promise<void>; result = { books, sessionOps } | { cancelled: true }
                                //   books: reorderBooks by array order, then saveBook each, then deleteBook for any id gone
                                //   sessionOps: appendSession / updateSession / deleteSession per op
  CONFIG_BASE_URL               // 'https://<user>.github.io/timereader-pebble/' (placeholder; set in Task 11)
  ```

- [x] **Step 1: Write the failing tests** — `tests/js/test_index_bridge.mjs`

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import ds from '../../watchapp/src/pkjs/datastore.js';
import bridge from '../../watchapp/src/pkjs/index.js';

function fakeStorage() {
  const m = new Map();
  return { getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)) };
}

test('buildConfigPayload groups sessions by book and computes globalPph', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A', total_pages: 100 });
  await ds.appendSession({ id: 's1', book_id: a.id, start_page: 1, end_page: 20, pages: 20, duration_seconds: 3600, source: 'manual' });
  const p = await bridge.buildConfigPayload();
  assert.deepEqual(Object.keys(p.sessionsByBook), [a.id]);
  assert.equal(p.globalPph, 20);
});

test('applyConfigResult persists book edits, reorder and session ops', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  await bridge.applyConfigResult({
    books: [{ ...b, title: 'B2' }, { ...a }],
    sessionOps: [
      { op: 'add', session: { id: 's9', book_id: a.id, start_page: 1, end_page: 9, pages: 9, duration_seconds: 60, source: 'manual' } },
    ],
  });
  const books = await ds.getBooks();
  assert.deepEqual(books.map((x) => x.title), ['B2', 'A']);   // reordered
  assert.deepEqual((await ds.getSessions()).map((s) => s.id), ['s9']);
});

test('applyConfigResult deletes books dropped from the list', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  await bridge.applyConfigResult({ books: [{ ...a }], sessionOps: [] });
  assert.deepEqual((await ds.getBooks()).map((x) => x.id), [a.id]);
});

test('applyConfigResult ignores a cancelled result', async () => {
  ds._setStorage(fakeStorage());
  await ds.saveBook({ title: 'A' });
  await bridge.applyConfigResult({ cancelled: true });
  assert.equal((await ds.getBooks()).length, 1);
});
```

- [x] **Step 2: Run to verify it fails** — module missing.

- [x] **Step 3: Write `index.js` (config bridge portion; sync portion is Task 10)**

```js
var datastore = require('./datastore');
var library = require('./library');

var CONFIG_BASE_URL = 'https://EXAMPLE.github.io/timereader-pebble/'; // set for real in Task 11

function buildConfigPayload() {
  return Promise.all([datastore.getBooks(), datastore.getSessions()]).then(function (r) {
    var books = r[0], all = r[1];
    var byBook = {};
    books.forEach(function (b) { byBook[b.id] = []; });
    all.forEach(function (s) { (byBook[s.book_id] = byBook[s.book_id] || []).push(s); });
    return { books: books, sessionsByBook: byBook, globalPph: library.globalPagesPerHour(all) };
  });
}

function applyConfigResult(result) {
  if (!result || result.cancelled) return Promise.resolve();
  var books = result.books || [];
  var keepIds = books.map(function (b) { return b.id; });
  return datastore.getBooks().then(function (existing) {
    var gone = existing.filter(function (b) { return keepIds.indexOf(b.id) < 0; });
    var chain = Promise.resolve();
    gone.forEach(function (b) { chain = chain.then(function () { return datastore.deleteBook(b.id); }); });
    chain = chain.then(function () { return datastore.reorderBooks(keepIds); });
    books.forEach(function (b) { chain = chain.then(function () { return datastore.saveBook(b); }); });
    (result.sessionOps || []).forEach(function (op) {
      chain = chain.then(function () {
        if (op.op === 'add') return datastore.appendSession(op.session);
        if (op.op === 'update') return datastore.updateSession(op.session);
        if (op.op === 'delete') return datastore.deleteSession(op.id);
      });
    });
    return chain;
  });
}

var api = {
  buildConfigPayload: buildConfigPayload,
  applyConfigResult: applyConfigResult,
  CONFIG_BASE_URL: CONFIG_BASE_URL,
};

if (typeof Pebble !== 'undefined') {
  Pebble.addEventListener('showConfiguration', function () {
    buildConfigPayload().then(function (payload) {
      Pebble.openURL(CONFIG_BASE_URL + '#' + encodeURIComponent(JSON.stringify(payload)));
    });
  });
  Pebble.addEventListener('webviewclosed', function (e) {
    var result = {};
    try { result = JSON.parse(decodeURIComponent(e.response || '')); } catch (x) { result = {}; }
    applyConfigResult(result).then(function () {
      if (api.sendSnapshot) api.sendSnapshot();   // re-push to the watch if open (Task 10 defines it)
    });
  });
}

module.exports = api;
```

- [x] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [x] **Step 5: Commit**

```bash
git add watchapp/src/pkjs/index.js tests/js/test_index_bridge.mjs
git commit -m "feat: pkjs config bridge with datastore diff on webviewclosed"
```

---

## Task 8: `store_core` + `store.c` — books cache with atomic shadow commit

Watch-side persistence for the digested book records. One record per numbered persist key (a `DigestBook` is ~90 B; `PERSIST_DATA_MAX_LENGTH` is 256 B). The shadow area stages an incoming snapshot; commit swaps it in atomically.

**Files:**
- Create: `watchapp/src/c/store_core.h`, `watchapp/src/c/store_core.c`, `tests/c/test_store_core.c`
- Modify: `watchapp/src/c/store.h`, `watchapp/src/c/store.c`, `watchapp/src/c/model.h`

**Interfaces:**
- `model.h` adds:
  ```c
  typedef struct {
    char id[16];
    char book_id[12];
    int start_page;
    int end_page;
    int duration_seconds;
  } QueuedSession;
  ```
- `store_core.h` (pure, no `<pebble.h>`):
  ```c
  #define STORE_MAX_BOOKS 24
  #define STORE_MAX_QUEUE 50
  /* Fixed-width serialisation so a record round-trips through a byte blob. */
  int  store_core_pack_book(const DigestBook *b, unsigned char *buf, int buf_size);   /* returns bytes written, 0 on overflow */
  int  store_core_unpack_book(const unsigned char *buf, int len, DigestBook *out);    /* returns 1 on success */
  int  store_core_pack_session(const QueuedSession *s, unsigned char *buf, int buf_size);
  int  store_core_unpack_session(const unsigned char *buf, int len, QueuedSession *out);
  /* Key layout helpers. */
  int  store_core_book_key(int index);       /* STORE_KEY_BOOKS_BASE + index */
  int  store_core_shadow_key(int index);     /* STORE_KEY_SHADOW_BASE + index */
  int  store_core_queue_key(int index);      /* STORE_KEY_QUEUE_BASE + index */
  ```
  with `#define STORE_KEY_BOOKS_BASE 100`, `STORE_KEY_SHADOW_BASE 140`, `STORE_KEY_QUEUE_BASE 180`, and count/misc keys `STORE_KEY_BOOKS_COUNT 3`, `STORE_KEY_SHADOW_COUNT 4`, `STORE_KEY_QUEUE_COUNT 5`, `STORE_KEY_SESSION_SEQ 6`, `STORE_KEY_SCHEMA_VERSION 7`.
- `store.h` adds (PebbleOS side, uses `store_core` + `persist_*`):
  ```c
  int  store_books_count(void);
  int  store_books_load(DigestBook *out, int max);         /* fills out[0..count), returns count */
  void store_shadow_begin(int count);                      /* clears shadow, records expected count */
  void store_shadow_put(int index, const DigestBook *b);
  void store_shadow_commit(void);                          /* shadow -> cache (all keys), then clears shadow + count */
  void store_shadow_discard(void);
  ```

- [x] **Step 1: Write the failing tests** — `tests/c/test_store_core.c`

```c
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
  strcpy(b.id, "b_000000000");
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
  CHECK_EQ_INT(out.start_page, 49, "");
  CHECK_EQ_INT(out.end_page, 61, "");
  CHECK_EQ_INT(out.duration_seconds, 733, "");
}

void t_key_layout_is_disjoint(void) {
  CHECK(store_core_book_key(0) != store_core_shadow_key(0), "");
  CHECK(store_core_shadow_key(STORE_MAX_BOOKS - 1) < store_core_queue_key(0), "no overlap");
}

TEST_BEGIN()
  t_book_roundtrip(); t_book_pack_fits_persist_limit();
  t_session_roundtrip(); t_key_layout_is_disjoint();
TEST_END()
```

- [x] **Step 2: Extend `tests/c/run.sh`**

In the impl loop, add `store_core sync_core` to the file list:
```bash
for f in digit_entry session state_machine store_core sync_core; do
  [ -f "$SRC/$f.c" ] && IMPL="$IMPL $SRC/$f.c"
done
```

- [x] **Step 3: Run to verify it fails** — `cd tests/c && ./run.sh` (missing `store_core.h`).

- [x] **Step 4: Add `QueuedSession` to `model.h`**

Exactly the struct in Interfaces, after `Session`.

- [x] **Step 5: Write `store_core.h` / `store_core.c`**

Fixed-width little-endian packing. Layout for a book: `id[16] title[48] int32 current_page, total_pages, pph_x100, hours_x100 uint8 color, pph_is_estimate, favorite` = 16 + 48 + 16 + 3 = 83 bytes. Session: `id[16] book_id[12] int32 start_page, end_page, duration_seconds` = 46 bytes. Implement with `memcpy` of the char arrays and manual byte writes for the ints (no struct packing assumptions):

```c
static void put32(unsigned char *p, int v) {
  p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}
static int get32(const unsigned char *p) {
  return (int)((unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24));
}
```
`store_core_book_key(i)` returns `STORE_KEY_BOOKS_BASE + i`, etc. Guard `index` against the `STORE_MAX_*` bounds (return -1 if out of range).

- [x] **Step 6: Run to verify it passes** — `cd tests/c && ./run.sh`.

- [x] **Step 7: Implement the `store.c` cache functions**

```c
int store_books_count(void) {
  return persist_exists(STORE_KEY_BOOKS_COUNT) ? persist_read_int(STORE_KEY_BOOKS_COUNT) : 0;
}
int store_books_load(DigestBook *out, int max) {
  int n = store_books_count();
  if (n > max) n = max;
  int got = 0;
  for (int i = 0; i < n; i++) {
    unsigned char buf[128];
    int len = persist_read_data(store_core_book_key(i), buf, sizeof(buf));
    if (len > 0 && store_core_unpack_book(buf, len, &out[got])) got++;
  }
  return got;
}
void store_shadow_begin(int count) {
  for (int i = 0; i < STORE_MAX_BOOKS; i++) {
    if (persist_exists(store_core_shadow_key(i))) persist_delete(store_core_shadow_key(i));
  }
  persist_write_int(STORE_KEY_SHADOW_COUNT, count);
}
void store_shadow_put(int index, const DigestBook *b) {
  if (index < 0 || index >= STORE_MAX_BOOKS) return;
  unsigned char buf[128];
  int n = store_core_pack_book(b, buf, sizeof(buf));
  if (n > 0) persist_write_data(store_core_shadow_key(index), buf, n);
}
void store_shadow_commit(void) {
  int count = persist_exists(STORE_KEY_SHADOW_COUNT) ? persist_read_int(STORE_KEY_SHADOW_COUNT) : 0;
  /* copy shadow -> cache */
  for (int i = 0; i < STORE_MAX_BOOKS; i++) {
    int ck = store_core_book_key(i), sk = store_core_shadow_key(i);
    if (i < count && persist_exists(sk)) {
      unsigned char buf[128];
      int len = persist_read_data(sk, buf, sizeof(buf));
      persist_write_data(ck, buf, len);
    } else if (persist_exists(ck)) {
      persist_delete(ck);
    }
  }
  persist_write_int(STORE_KEY_BOOKS_COUNT, count);
  store_shadow_discard();
}
void store_shadow_discard(void) {
  for (int i = 0; i < STORE_MAX_BOOKS; i++) {
    if (persist_exists(store_core_shadow_key(i))) persist_delete(store_core_shadow_key(i));
  }
  if (persist_exists(STORE_KEY_SHADOW_COUNT)) persist_delete(STORE_KEY_SHADOW_COUNT);
}
```
`#include "store_core.h"` in `store.c`.

- [x] **Step 8: Build check** — `cd watchapp && pebble build` (no errors; functions unused until Task 12).

- [x] **Step 9: Commit**

```bash
git add watchapp/src/c/store_core.* watchapp/src/c/store.* watchapp/src/c/model.h tests/c/test_store_core.c tests/c/run.sh
git commit -m "feat: watch books cache with fixed-width records and atomic shadow commit"
```

---

## Task 9: `store.c` — persistent session queue + session-id counter

**Files:**
- Modify: `watchapp/src/c/store.h`, `watchapp/src/c/store.c`
- Test: `tests/c/test_store_core.c` (append queue-index tests only — the `persist_*` glue is exercised end-to-end in Task 15)

**Interfaces:**
- `store.h` adds:
  ```c
  int  store_queue_count(void);
  int  store_queue_load(QueuedSession *out, int max);         /* oldest first; returns count */
  bool store_queue_push(const QueuedSession *s);              /* false if full (STORE_MAX_QUEUE) */
  void store_queue_remove(const char *id);                    /* drop the entry with this id, compact */
  int  store_next_session_seq(void);                          /* monotonic counter in persist, for unique ids */
  ```

- [x] **Step 1: Append a `store_core` test for compaction math**

```c
void t_queue_compaction_shifts_tail_down(void) {
  /* store_core exposes the shift helper used by store_queue_remove */
  QueuedSession q[3];
  for (int i = 0; i < 3; i++) { memset(&q[i], 0, sizeof(q[i])); q[i].start_page = i; }
  int n = store_core_queue_drop(q, 3, 1);   /* remove index 1 */
  CHECK_EQ_INT(n, 2, "");
  CHECK_EQ_INT(q[0].start_page, 0, "");
  CHECK_EQ_INT(q[1].start_page, 2, "tail shifted down");
}
```
Add `int store_core_queue_drop(QueuedSession *q, int n, int remove_index);` to `store_core.h/.c` (pure array compaction, returns new count). Register the test in `TEST_BEGIN`.

- [x] **Step 2: Run to verify it fails** — `cd tests/c && ./run.sh`.

- [x] **Step 3: Implement `store_core_queue_drop`** — memmove the tail down, return `n - 1`.

- [x] **Step 4: Run to verify it passes** — `cd tests/c && ./run.sh`.

- [x] **Step 5: Implement the `store.c` queue functions**

```c
int store_queue_count(void) {
  return persist_exists(STORE_KEY_QUEUE_COUNT) ? persist_read_int(STORE_KEY_QUEUE_COUNT) : 0;
}
int store_queue_load(QueuedSession *out, int max) {
  int n = store_queue_count(); if (n > max) n = max;
  int got = 0;
  for (int i = 0; i < n; i++) {
    unsigned char buf[64];
    int len = persist_read_data(store_core_queue_key(i), buf, sizeof(buf));
    if (len > 0 && store_core_unpack_session(buf, len, &out[got])) got++;
  }
  return got;
}
bool store_queue_push(const QueuedSession *s) {
  int n = store_queue_count();
  if (n >= STORE_MAX_QUEUE) return false;
  unsigned char buf[64];
  int len = store_core_pack_session(s, buf, sizeof(buf));
  if (len <= 0) return false;
  persist_write_data(store_core_queue_key(n), buf, len);
  persist_write_int(STORE_KEY_QUEUE_COUNT, n + 1);
  return true;
}
void store_queue_remove(const char *id) {
  QueuedSession q[STORE_MAX_QUEUE];
  int n = store_queue_load(q, STORE_MAX_QUEUE);
  int idx = -1;
  for (int i = 0; i < n; i++) if (strcmp(q[i].id, id) == 0) { idx = i; break; }
  if (idx < 0) return;
  n = store_core_queue_drop(q, n, idx);
  for (int i = 0; i < n; i++) {
    unsigned char buf[64];
    int len = store_core_pack_session(&q[i], buf, sizeof(buf));
    persist_write_data(store_core_queue_key(i), buf, len);
  }
  if (persist_exists(store_core_queue_key(n))) persist_delete(store_core_queue_key(n));
  persist_write_int(STORE_KEY_QUEUE_COUNT, n);
}
int store_next_session_seq(void) {
  int v = persist_exists(STORE_KEY_SESSION_SEQ) ? persist_read_int(STORE_KEY_SESSION_SEQ) : 0;
  persist_write_int(STORE_KEY_SESSION_SEQ, v + 1);
  return v;
}
```

- [x] **Step 6: Build check** — `cd watchapp && pebble build`.

- [x] **Step 7: Commit**

```bash
git add watchapp/src/c/store.* watchapp/src/c/store_core.* tests/c/test_store_core.c
git commit -m "feat: persistent watch session queue with idempotent removal"
```

---

## Task 10: `sync_core` — pure snapshot/queue/ACK/retract logic

The decision core, no `<pebble.h>`. It talks to the outside world through two vtables the caller supplies: a store and a transport. `sync.c` (Task 11) wires the real ones; the tests wire fakes.

**Files:**
- Create: `watchapp/src/c/sync_core.h`, `watchapp/src/c/sync_core.c`, `tests/c/test_sync_core.c`, `tests/c/fake_sync_env.h`

**Interfaces:**
- `sync_core.h`:
  ```c
  typedef struct {
    void (*shadow_begin)(void *ctx, int count);
    void (*shadow_put)(void *ctx, int idx, const DigestBook *b);
    void (*shadow_commit)(void *ctx);
    void (*shadow_discard)(void *ctx);
    int  (*queue_load)(void *ctx, QueuedSession *out, int max);
    bool (*queue_push)(void *ctx, const QueuedSession *s);
    void (*queue_remove)(void *ctx, const char *id);
  } SyncStore;

  typedef struct {
    /* returns true if the message was accepted for sending (one in flight) */
    bool (*send_session)(void *ctx, const QueuedSession *s);
    bool (*send_retract)(void *ctx, const char *id);
  } SyncTransport;

  typedef struct {
    const SyncStore *store; void *store_ctx;
    const SyncTransport *tx; void *tx_ctx;
    bool snapshot_active;
    int  snapshot_expected;
    int  snapshot_seen;
    bool awaiting_ack;
    char inflight_id[16];
    char pending_retract[16];   /* non-empty => a retract waiting for RETRACT_ACK */
    void (*on_books_changed)(void *ui_ctx);
    void *ui_ctx;
  } SyncCore;

  void sync_core_init(SyncCore *c, const SyncStore *s, void *sctx,
                      const SyncTransport *t, void *tctx,
                      void (*on_books_changed)(void *), void *ui_ctx);
  void sync_core_snapshot_begin(SyncCore *c, int count);
  void sync_core_snapshot_book(SyncCore *c, int idx, const DigestBook *b);
  void sync_core_snapshot_end(SyncCore *c);
  void sync_core_snapshot_abort(SyncCore *c);           /* pkjs disconnect / app exit mid-snapshot */
  void sync_core_enqueue(SyncCore *c, const QueuedSession *s);   /* push + try to send */
  void sync_core_drain(SyncCore *c);                    /* send the head of the queue if idle */
  void sync_core_on_session_ack(SyncCore *c, const char *id);
  void sync_core_on_retract_ack(SyncCore *c, const char *id);
  void sync_core_retract(SyncCore *c, const char *id);  /* remove from queue; if already sent, send SESSION_RETRACT */
  void sync_core_on_send_result(SyncCore *c, bool ok);  /* APP_MSG_OK / failed for the in-flight message */
  ```
- Behaviour contract:
  - `snapshot_begin` → `store.shadow_begin(count)`, `snapshot_active = true`, counters reset.
  - `snapshot_book` while active → `store.shadow_put(idx, b)`, `snapshot_seen++`.
  - `snapshot_end` while active → `store.shadow_commit()`, `snapshot_active = false`, `on_books_changed()` fires. Then `sync_core_drain()`.
  - `snapshot_abort` or a `snapshot_begin` while one is already active → `store.shadow_discard()` on the stale one first.
  - `enqueue` → `store.queue_push(s)`; if `!awaiting_ack` call `drain`.
  - `drain` → if `awaiting_ack` or snapshot active, do nothing; else `queue_load` head; if present, `tx.send_session(head)`; on true → `awaiting_ack = true`, copy `inflight_id`.
  - `on_send_result(false)` → `awaiting_ack = false` (retry on next drain / launch); keep the queue entry.
  - `on_session_ack(id)` → if `id == inflight_id`: `store.queue_remove(id)`, `awaiting_ack = false`, `drain` for the next. An ACK for an already-removed id is ignored (idempotent).
  - `retract(id)` → if `id` is in the queue: `store.queue_remove(id)`; if `id == inflight_id` also clear `awaiting_ack`. If `id` is **not** in the queue (already ACKed and gone): `tx.send_retract(id)`, set `pending_retract`.
  - `on_retract_ack(id)` → clear `pending_retract` if it matches.

- [x] **Step 1: Write `tests/c/fake_sync_env.h`**

An in-memory `FakeEnv` with: a `DigestBook cache[STORE_MAX_BOOKS]` + `shadow[...]` + counts, a `QueuedSession queue[STORE_MAX_QUEUE]` + count, `sent_sessions` / `sent_retracts` logs, and `SyncStore` / `SyncTransport` instances whose function pointers manipulate the `FakeEnv`. `send_session` / `send_retract` push to the log and return `true` unless `env->tx_offline` is set.

- [x] **Step 2: Write the failing tests** — `tests/c/test_sync_core.c`

```c
#include "sync_core.h"
#include "fake_sync_env.h"
#include "test.h"
#include <string.h>

static QueuedSession mk(const char *id) {
  QueuedSession s; memset(&s, 0, sizeof(s));
  strcpy(s.id, id); strcpy(s.book_id, "b1"); s.start_page = 10; s.end_page = 20; s.duration_seconds = 600;
  return s;
}

void t_snapshot_commits_atomically(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  DigestBook b; memset(&b, 0, sizeof(b)); strcpy(b.id, "b1"); strcpy(b.title, "One");
  sync_core_snapshot_begin(&c, 1);
  sync_core_snapshot_book(&c, 0, &b);
  CHECK_EQ_INT(e.cache_count, 0, "not visible mid-snapshot");
  sync_core_snapshot_end(&c);
  CHECK_EQ_INT(e.cache_count, 1, "committed on END");
  CHECK(e.on_books_changed_calls == 1, "UI told once");
}

void t_interrupted_snapshot_keeps_old_cache(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  fake_env_seed_cache(&e, 2);                 /* two good books already cached */
  sync_core_snapshot_begin(&c, 5);
  DigestBook b; memset(&b, 0, sizeof(b)); strcpy(b.id, "x");
  sync_core_snapshot_book(&c, 0, &b);
  sync_core_snapshot_abort(&c);               /* BT drops */
  CHECK_EQ_INT(e.cache_count, 2, "old cache intact");
  CHECK_EQ_INT(e.shadow_count, 0, "shadow discarded");
}

void t_queue_sends_one_and_waits_for_ack(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  sync_core_enqueue(&c, mk("s1"));
  sync_core_enqueue(&c, mk("s2"));
  CHECK_EQ_INT(e.sent_count, 1, "only the head is in flight");
  CHECK_EQ_STR(e.sent[0].id, "s1", "");
  sync_core_on_send_result(&c, true);
  sync_core_on_session_ack(&c, "s1");
  CHECK_EQ_INT(e.sent_count, 2, "next flows after ACK");
  CHECK_EQ_STR(e.sent[1].id, "s2", "");
}

void t_ack_is_idempotent(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  sync_core_enqueue(&c, mk("s1"));
  sync_core_on_send_result(&c, true);
  sync_core_on_session_ack(&c, "s1");
  sync_core_on_session_ack(&c, "s1");         /* duplicate ACK */
  CHECK_EQ_INT(e.queue_count, 0, "still empty, no crash");
}

void t_lost_ack_retries_on_next_drain(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  sync_core_enqueue(&c, mk("s1"));
  sync_core_on_send_result(&c, true);
  /* no ACK arrives; a later launch calls drain again */
  sync_core_on_send_result(&c, false);        /* previous attempt considered failed */
  sync_core_drain(&c);
  CHECK_EQ_INT(e.sent_count, 2, "re-sent s1");
  CHECK_EQ_STR(e.sent[1].id, "s1", "");
}

void t_retract_before_ack_just_drops_locally(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  sync_core_enqueue(&c, mk("s1"));
  sync_core_retract(&c, "s1");
  CHECK_EQ_INT(e.queue_count, 0, "");
  CHECK_EQ_INT(e.sent_retract_count, 0, "phone never saw it, no retract message");
}

void t_retract_after_ack_sends_retract_message(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  sync_core_enqueue(&c, mk("s1"));
  sync_core_on_send_result(&c, true);
  sync_core_on_session_ack(&c, "s1");         /* gone from the queue */
  sync_core_retract(&c, "s1");
  CHECK_EQ_INT(e.sent_retract_count, 1, "");
  CHECK_EQ_STR(e.sent_retracts[0], "s1", "");
  sync_core_on_retract_ack(&c, "s1");
  CHECK_EQ_INT(c.pending_retract[0], 0, "cleared on RETRACT_ACK");
}

TEST_BEGIN()
  t_snapshot_commits_atomically();
  t_interrupted_snapshot_keeps_old_cache();
  t_queue_sends_one_and_waits_for_ack();
  t_ack_is_idempotent();
  t_lost_ack_retries_on_next_drain();
  t_retract_before_ack_just_drops_locally();
  t_retract_after_ack_sends_retract_message();
TEST_END()
```

- [x] **Step 3: Run to verify it fails** — `cd tests/c && ./run.sh`.

- [x] **Step 4: Write `sync_core.c`** following the behaviour contract above. Keep every branch small; no allocation; `strncpy` into the fixed `inflight_id` / `pending_retract` buffers.

- [x] **Step 5: Run to verify it passes** — `cd tests/c && ./run.sh`. Confirm the full C suite (SP1 + SP2) is green.

- [x] **Step 6: Commit**

```bash
git add watchapp/src/c/sync_core.* tests/c/test_sync_core.c tests/c/fake_sync_env.h
git commit -m "feat: pure sync core for snapshot commit, session queue and retract"
```

---

## Task 11: `sync.c` — PebbleOS AppMessage adapter + `package.json` keys

**Files:**
- Create: `watchapp/src/c/sync.h`, `watchapp/src/c/sync.c`
- Modify: `watchapp/package.json`

**Interfaces:**
- `package.json` `messageKeys` (exact order — indices matter only for docs, names are what the code uses):
  ```json
  "messageKeys": [
    "SNAPSHOT_BEGIN", "SNAPSHOT_END",
    "BOOK_IDX", "BOOK_ID", "BOOK_TITLE", "BOOK_COLOR", "BOOK_CUR_PAGE",
    "BOOK_TOT_PAGES", "BOOK_PPH_X100", "BOOK_HOURS_X100", "BOOK_FLAGS",
    "SESSION_ID", "SESSION_BOOK_ID", "SESSION_START_PAGE", "SESSION_END_PAGE", "SESSION_DURATION_S",
    "SESSION_ACK_ID", "SESSION_RETRACT_ID", "RETRACT_ACK_ID"
  ]
  ```
  Also add `"enableMultiJS": true` under `pebble`.
- `sync.h`:
  ```c
  void sync_init(void (*on_books_changed)(void));   /* register AppMessage, open inbox/outbox */
  void sync_enqueue_session(const QueuedSession *s);
  void sync_retract_session(const char *id);
  void sync_drain(void);                            /* call on launch and on a slow tick */
  int  sync_books_into(DigestBook *out, int max);   /* pass-through to store_books_load */
  ```

- [x] **Step 1: Update `package.json`** with the keys and `enableMultiJS`, then `cd watchapp && pebble build`. Expected: the generated `build/**/message_keys.*` now lists the keys; build succeeds.

- [x] **Step 2: Write `sync.c`**

Bridges AppMessage ⇄ `sync_core`. Sketch:
```c
#include <pebble.h>
#include "sync.h"
#include "sync_core.h"
#include "store.h"

static SyncCore s_core;
static void (*s_on_books_changed_ui)(void);
static void ui_books_changed(void *ctx) { if (s_on_books_changed_ui) s_on_books_changed_ui(); }

/* --- SyncStore over store.c --- */
static void st_shadow_begin(void *c, int n) { store_shadow_begin(n); }
static void st_shadow_put(void *c, int i, const DigestBook *b) { store_shadow_put(i, b); }
static void st_shadow_commit(void *c) { store_shadow_commit(); }
static void st_shadow_discard(void *c) { store_shadow_discard(); }
static int  st_queue_load(void *c, QueuedSession *o, int m) { return store_queue_load(o, m); }
static bool st_queue_push(void *c, const QueuedSession *s) { return store_queue_push(s); }
static void st_queue_remove(void *c, const char *id) { store_queue_remove(id); }
static const SyncStore STORE_VT = { st_shadow_begin, st_shadow_put, st_shadow_commit, st_shadow_discard,
                                    st_queue_load, st_queue_push, st_queue_remove };

/* --- SyncTransport over app_message --- */
static bool tx_send_session(void *c, const QueuedSession *s) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return false;
  dict_write_cstring(it, MESSAGE_KEY_SESSION_ID, s->id);
  dict_write_cstring(it, MESSAGE_KEY_SESSION_BOOK_ID, s->book_id);
  dict_write_int32(it, MESSAGE_KEY_SESSION_START_PAGE, s->start_page);
  dict_write_int32(it, MESSAGE_KEY_SESSION_END_PAGE, s->end_page);
  dict_write_int32(it, MESSAGE_KEY_SESSION_DURATION_S, s->duration_seconds);
  return app_message_outbox_send() == APP_MSG_OK;
}
static bool tx_send_retract(void *c, const char *id) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return false;
  dict_write_cstring(it, MESSAGE_KEY_SESSION_RETRACT_ID, id);
  return app_message_outbox_send() == APP_MSG_OK;
}
static const SyncTransport TX_VT = { tx_send_session, tx_send_retract };

static void inbox_received(DictionaryIterator *it, void *ctx) {
  Tuple *t;
  if ((t = dict_find(it, MESSAGE_KEY_SNAPSHOT_BEGIN))) { sync_core_snapshot_begin(&s_core, t->value->int32); return; }
  if ((t = dict_find(it, MESSAGE_KEY_SNAPSHOT_END)))   { sync_core_snapshot_end(&s_core); return; }
  if ((t = dict_find(it, MESSAGE_KEY_BOOK_IDX))) {
    DigestBook b; memset(&b, 0, sizeof(b));
    int idx = t->value->int32;
    Tuple *x;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_ID)))       strncpy(b.id, x->value->cstring, sizeof(b.id) - 1);
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_TITLE)))    strncpy(b.title, x->value->cstring, sizeof(b.title) - 1);
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_COLOR)))    b.color = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_CUR_PAGE))) b.current_page = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_TOT_PAGES)))b.total_pages = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_PPH_X100))) b.pph_x100 = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_HOURS_X100)))b.hours_x100 = x->value->int32;
    if ((x = dict_find(it, MESSAGE_KEY_BOOK_FLAGS))) {
      int f = x->value->int32;
      b.pph_is_estimate = f & 1; b.favorite = f & 2;
    }
    sync_core_snapshot_book(&s_core, idx, &b);
    return;
  }
  if ((t = dict_find(it, MESSAGE_KEY_SESSION_ACK_ID)))  { sync_core_on_session_ack(&s_core, t->value->cstring); return; }
  if ((t = dict_find(it, MESSAGE_KEY_RETRACT_ACK_ID)))  { sync_core_on_retract_ack(&s_core, t->value->cstring); return; }
}
static void outbox_sent(DictionaryIterator *it, void *ctx)       { sync_core_on_send_result(&s_core, true); }
static void outbox_failed(DictionaryIterator *it, AppMessageResult r, void *ctx) { sync_core_on_send_result(&s_core, false); }

void sync_init(void (*on_books_changed)(void)) {
  s_on_books_changed_ui = on_books_changed;
  sync_core_init(&s_core, &STORE_VT, NULL, &TX_VT, NULL, ui_books_changed, NULL);
  app_message_register_inbox_received(inbox_received);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(512, 128);
}
void sync_enqueue_session(const QueuedSession *s) { sync_core_enqueue(&s_core, s); }
void sync_retract_session(const char *id) { sync_core_retract(&s_core, id); }
void sync_drain(void) { sync_core_drain(&s_core); }
int  sync_books_into(DigestBook *out, int max) { return store_books_load(out, max); }
```
`app_message_open` inbox size 512 covers a `BOOK` record (id+title+7 ints ≈ 130 B plus dict overhead).

- [ ] **Step 3: Build check** — `cd watchapp && pebble build`. Expected: compiles; `MESSAGE_KEY_*` resolve from the generated header.

- [ ] **Step 4: Commit**

```bash
git add watchapp/src/c/sync.* watchapp/package.json
git commit -m "feat: AppMessage adapter wiring sync_core to PebbleOS"
```

---

## Task 12: `index.js` — snapshot sender + inbound session handlers

**Files:**
- Modify: `watchapp/src/pkjs/index.js`
- Test: `tests/js/test_index_bridge.mjs` (append)

**Interfaces:**
- Produces (added to `module.exports`):
  ```js
  buildSnapshotMessages()      // -> Promise<Array<object>>  the ordered AppMessage dicts:
                               //    [{SNAPSHOT_BEGIN: n}, {BOOK_IDX:0, BOOK_ID:..., ...}, ..., {SNAPSHOT_END: 1}]
  handleSessionMessage(dict)   // -> Promise<{SESSION_ACK_ID}>   appendSession (idempotent) then ack
  handleRetractMessage(dict)   // -> Promise<{RETRACT_ACK_ID}>   deleteSession then ack
  ```
  The real file also wires `sendSnapshot()` (sends the array one dict at a time, each after the previous `ACK`/`NACK` callback) and the `appmessage` listener.

- [x] **Step 1: Append failing tests**

```js
test('buildSnapshotMessages brackets the books with BEGIN/END', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A', total_pages: 10 });
  await ds.appendSession({ id: 's1', book_id: a.id, start_page: 1, end_page: 10, pages: 10, duration_seconds: 3600, source: 'manual' });
  const msgs = await bridge.buildSnapshotMessages();
  assert.equal(msgs[0].SNAPSHOT_BEGIN, 1);
  assert.equal(msgs[1].BOOK_IDX, 0);
  assert.equal(msgs[1].BOOK_ID, a.id);
  assert.equal(msgs[1].BOOK_COLOR, 2);           // 10/10 -> completed
  assert.equal(msgs[2].SNAPSHOT_END, 1);
});

test('handleSessionMessage appends idempotently and acks', async () => {
  ds._setStorage(fakeStorage());
  const dict = { SESSION_ID: 's9', SESSION_BOOK_ID: 'b1', SESSION_START_PAGE: 10, SESSION_END_PAGE: 22, SESSION_DURATION_S: 600 };
  const r1 = await bridge.handleSessionMessage(dict);
  const r2 = await bridge.handleSessionMessage(dict);
  assert.equal(r1.SESSION_ACK_ID, 's9');
  assert.equal(r2.SESSION_ACK_ID, 's9');
  assert.equal((await ds.getSessions()).length, 1);
  assert.equal((await ds.getSessions())[0].pages, 13);   // inclusive 22-10+1
});

test('handleRetractMessage deletes and acks', async () => {
  ds._setStorage(fakeStorage());
  await ds.appendSession({ id: 's9', book_id: 'b1', start_page: 1, end_page: 5, pages: 5, duration_seconds: 60, source: 'watch' });
  const r = await bridge.handleRetractMessage({ SESSION_RETRACT_ID: 's9' });
  assert.equal(r.RETRACT_ACK_ID, 's9');
  assert.equal((await ds.getSessions()).length, 0);
});
```

- [x] **Step 2: Run to verify it fails** — `cd tests/js && ./run.sh`.

- [x] **Step 3: Implement in `index.js`**

```js
function buildSnapshotMessages() {
  return buildConfigPayload().then(function (p) {
    var snap = library.computeSnapshot(p.books, p.sessionsByBook);
    var msgs = [{ SNAPSHOT_BEGIN: snap.length }];
    snap.forEach(function (d, i) {
      msgs.push({
        BOOK_IDX: i, BOOK_ID: d.id, BOOK_TITLE: (d.title || '').slice(0, 47),
        BOOK_COLOR: d.color, BOOK_CUR_PAGE: d.cur_page, BOOK_TOT_PAGES: d.tot_pages,
        BOOK_PPH_X100: d.pph_x100, BOOK_HOURS_X100: d.hours_x100, BOOK_FLAGS: d.flags,
      });
    });
    msgs.push({ SNAPSHOT_END: 1 });
    return msgs;
  });
}
function handleSessionMessage(dict) {
  var s = {
    id: dict.SESSION_ID, book_id: dict.SESSION_BOOK_ID,
    start_page: dict.SESSION_START_PAGE, end_page: dict.SESSION_END_PAGE,
    pages: dict.SESSION_END_PAGE - dict.SESSION_START_PAGE + 1,
    duration_seconds: dict.SESSION_DURATION_S, source: 'watch',
  };
  return datastore.appendSession(s).then(function () { return { SESSION_ACK_ID: s.id }; });
}
function handleRetractMessage(dict) {
  return datastore.deleteSession(dict.SESSION_RETRACT_ID)
    .then(function () { return { RETRACT_ACK_ID: dict.SESSION_RETRACT_ID }; });
}
function sendSnapshot() {
  buildSnapshotMessages().then(function (msgs) {
    (function step(i) {
      if (i >= msgs.length) return;
      Pebble.sendAppMessage(msgs[i], function () { step(i + 1); }, function () { step(i + 1); });
    })(0);
  });
}
api.buildSnapshotMessages = buildSnapshotMessages;
api.handleSessionMessage = handleSessionMessage;
api.handleRetractMessage = handleRetractMessage;
api.sendSnapshot = sendSnapshot;

if (typeof Pebble !== 'undefined') {
  Pebble.addEventListener('ready', function () { sendSnapshot(); });
  Pebble.addEventListener('appmessage', function (e) {
    var d = e.payload || {};
    if (d.SESSION_ID !== undefined) {
      handleSessionMessage(d).then(function (ack) { Pebble.sendAppMessage(ack); });
    } else if (d.SESSION_RETRACT_ID !== undefined) {
      handleRetractMessage(d).then(function (ack) { Pebble.sendAppMessage(ack); });
    }
  });
}
```

- [x] **Step 4: Run to verify it passes** — `cd tests/js && ./run.sh`.

- [x] **Step 5: Commit**

```bash
git add watchapp/src/pkjs/index.js tests/js/test_index_bridge.mjs
git commit -m "feat: pkjs snapshot sender and inbound session/retract handlers"
```

---

## Task 13: Wire the watchapp to the cache and the queue

**Files:**
- Modify: `watchapp/src/c/seed.c`, `watchapp/src/c/seed.h`, `watchapp/src/c/ui_common.c`, `watchapp/src/c/main.c`

**Interfaces:**
- Consumes: `sync.h`, `store.h`.
- `seed.h` becomes:
  ```c
  /* Now serves the sync cache. Returns a pointer to a static buffer filled
     from store_books_load(); *count_out is 0 when the phone has never
     synced (the state machine then shows APP_NO_BOOKS). */
  const DigestBook *seed_books(int *count_out);
  void seed_reload(void);   /* re-read the cache after a snapshot commit */
  ```

- [x] **Step 1: Rewrite `seed.c`**

```c
#include "seed.h"
#include "store.h"
#include "store_core.h"

static DigestBook s_books[STORE_MAX_BOOKS];
static int s_count = -1;

void seed_reload(void) { s_count = store_books_load(s_books, STORE_MAX_BOOKS); }

const DigestBook *seed_books(int *count_out) {
  if (s_count < 0) seed_reload();
  *count_out = s_count;
  return s_books;
}
```
Delete the hard-coded book array. (The long-title / estimate cases from SP1 now come from real data entered in the config page.)

- [x] **Step 2: Feed the queue on save / retract** — in `ui_common.c` `ui_dispatch`:

```c
    case FX_SAVE_SESSION: {
      QueuedSession qs;
      memset(&qs, 0, sizeof(qs));
      snprintf(qs.id, sizeof(qs.id), "w%d", store_next_session_seq());
      int bn; const DigestBook *bk = ui_books(&bn);
      if (g_ctx.book_index >= 0 && g_ctx.book_index < bn) {
        strncpy(qs.book_id, bk[g_ctx.book_index].id, sizeof(qs.book_id) - 1);
      }
      qs.start_page = g_ctx.start_page_for_session;
      qs.end_page = digit_entry_value(&g_ctx.entry);
      qs.duration_seconds = g_ctx.last_session_seconds;
      strncpy(g_last_saved_session_id, qs.id, sizeof(g_last_saved_session_id) - 1);
      sync_enqueue_session(&qs);
      store_clear_session();
      break;
    }
    case FX_RETRACT_SESSION:
      sync_retract_session(g_last_saved_session_id);
      store_clear_session();
      break;
    case FX_DISCARD_SESSION:
      store_clear_session();
      break;
```
Add `char g_last_saved_session_id[16];` to `ui_common.c` (module-level) — SP1's retract only needs the id of the session it just enqueued. `#include "sync.h"`, `"store.h"`, `"digit_entry.h"` in `ui_common.c`.

- [x] **Step 3: Init sync and refresh on snapshot commit** — in `main.c`:

```c
static void on_books_changed(void) {
  seed_reload();
  int n; const DigestBook *b = seed_books(&n);
  /* rebuild the model around the new list, preserving a live session */
  PersistedSession p = store_load_session();
  int now = (int)time(NULL);
  if (p.present) sm_restore(&g_ctx, b, n, p.state, p.start_page, p.elapsed_seconds, now);
  else           sm_init(&g_ctx, b, n, now);
  ui_route_to_state();
  ui_refresh_current();
}

static void tick_handler(struct tm *t, TimeUnits u) {
  ui_dispatch(EV_TICK);
  if (u & MINUTE_UNIT) sync_drain();   /* retry the queue every minute while open */
}

static void init(void) {
  sync_init(on_books_changed);
  int n; const DigestBook *b = seed_books(&n);
  PersistedSession p = store_load_session();
  int now = (int)time(NULL);
  if (p.present) sm_restore(&g_ctx, b, n, p.state, p.start_page, p.elapsed_seconds, now);
  else           sm_init(&g_ctx, b, n, now);
  ui_route_to_state();
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  sync_drain();
}
```

- [x] **Step 4: Build + emulator smoke** — `cd watchapp && pebble build && pebble install --emulator basalt && pebble logs`. Expected: with an empty cache the watch shows "Nessun libro" (`APP_NO_BOOKS`); no crash.

- [x] **Step 5: Commit**

```bash
git add watchapp/src/c/seed.* watchapp/src/c/ui_common.c watchapp/src/c/main.c
git commit -m "feat: drive the watchapp from the sync cache and feed the session queue"
```

---

## Task 14: Config page — final polish + GitHub Pages setup

**Files:**
- Modify: `config-page/index.html`, `watchapp/src/pkjs/index.js` (`CONFIG_BASE_URL`)
- Create: `config-page/README.md`, `.github/workflows/pages.yml` (optional static deploy)

- [ ] **Step 1: Set the real config URL** — replace `CONFIG_BASE_URL` in `index.js` with `https://<github-user>.github.io/timereader-pebble/config-page/` (ask the user for their GitHub username; if unknown, leave a clearly-marked `TODO(user)` and note it in the commit body and HANDOFF).

- [ ] **Step 2: Polish the page** — dark/light tokens verified, focus styles, `<label>`s wired, buttons reachable by keyboard, the sessions panel and book form collapse cleanly. No functional change; keep all `tests/js` green.

- [x] **Step 3: Write `config-page/README.md`** — one paragraph: what the page is, that it is stateless and driven by the URL hash, how to preview it locally (`python3 -m http.server` then open with a hand-built hash), and that `pebble emu-app-config` drives the full cycle.

- [x] **Step 4: (Optional) `.github/workflows/pages.yml`** — deploy `config-page/` to GitHub Pages on push to `main`. If the user prefers manual Pages setup, skip this file and note it.

- [x] **Step 5: Run `tests/js` + commit**

```bash
cd tests/js && ./run.sh
git add config-page/ watchapp/src/pkjs/index.js .github/
git commit -m "feat: config page polish and GitHub Pages deployment"
```

---

## Task 15: End-to-end — `emu-app-config` cycle + AppMessage in emulator

**Files:**
- Modify: `docs/on-device-checklist.md`, `docs/HANDOFF.md`
- Create: `docs/sp2-e2e-notes.md`

- [x] **Step 1: Config round-trip in the emulator**

```bash
export PATH="$HOME/.local/bin:$PATH"
cd watchapp && pebble build && pebble install --emulator basalt
pebble emu-app-config --file ../config-page/index.html   # or the deployed URL once live
```
Add two books and one session in the page, hit "Salva". Verify from `pebble logs` that `webviewclosed` fired, `applyConfigResult` ran, and `sendSnapshot` pushed `SNAPSHOT_BEGIN` → `BOOK` × 2 → `SNAPSHOT_END`. Screenshot the watch: the book list now shows the two real books with the right colours. Document the exact commands and expected log lines in `docs/sp2-e2e-notes.md`.

- [x] **Step 2: Session queue round-trip**

Start a session on the watch, save an end page, land on the summary. From `pebble logs` confirm `sync_core_enqueue` → `SESSION` sent → pkjs `handleSessionMessage` → `SESSION_ACK` → queue emptied. Reopen the config page: the new session is listed under its book, and the book's current page / colour updated. Screenshot.

- [x] **Step 3: Retract round-trip**

Repeat, but press Up on the summary. Confirm: if the `SESSION_ACK` had already arrived, `SESSION_RETRACT` is sent and the session disappears from the config page; if not, the queue entry is just dropped and nothing is sent. Screenshot the paused timer it lands on.

- [x] **Step 4: Interrupted-snapshot check**

Trigger a snapshot, then `pebble kill` mid-stream (or toggle `pebble emu-bt-connection --state disconnected`). Relaunch: the previous book list is still there (shadow discarded, cache intact).

- [x] **Step 5: Update the checklist + handoff**

Append an "SP2" section to `docs/on-device-checklist.md` (config page adds/edits/deletes reach the watch on next open; a completed session appears in the config page; retract from summary removes it; queue survives a BT drop and drains on reconnect; interrupted snapshot keeps the old cache). Update `docs/HANDOFF.md`: SP2 done, branch `sp2-companion`, all tests green, note the `CONFIG_BASE_URL` value (or the pending `TODO(user)`), and the remaining SP2-d publishing steps (Rebble store + `.pbw` on GitHub Releases + sideload instructions) as the only open items.

- [x] **Step 6: Full-suite run**

```bash
cd tests/c && ./run.sh          # SP1 + store_core + sync_core all green
cd ../js && ./run.sh            # datastore + library + config page + index bridge all green
cd ../../watchapp && pebble build
```

- [x] **Step 7: Commit**

```bash
git add docs/
git commit -m "docs: SP2 end-to-end verification notes and checklist"
```

---

## Task 16: Distribution docs (SP2-d)

**Files:**
- Create: `docs/distribution.md`
- Modify: `README.md`

- [ ] **Step 1: Write `docs/distribution.md`**

Port the spec's "Distribuzione" section into a runnable checklist: building the release `.pbw` (`pebble build`, artefact at `watchapp/build/watchapp.pbw`), attaching it to a GitHub Release, sideloading via "Sideload Helper by Rebble" / the Pebble app, and submitting to the Rebble App Store (free). Note that Pebble Time is Bluetooth-only — no USB install — and that the config page updates independently of the `.pbw` as long as the `messageKeys` don't change.

- [ ] **Step 2: Update `README.md`** — add a "Distribution" pointer to the new doc and a one-line status: "SP1 + SP2 complete; publishing is manual per `docs/distribution.md`."

- [ ] **Step 3: Commit**

```bash
git add docs/distribution.md README.md
git commit -m "docs: distribution and sideload instructions"
```

---

## Self-Review

**1. Spec coverage:**
- "Vincolo strategico: offline-first" → `datastore.js` seam, Task 1; Global Constraints. ✓
- "Modello dati / Dove vive ogni pezzo" → books+sessions on phone (`datastore.js`, Tasks 1); live session + queue + selected book on watch (`store.c`, SP1 + Tasks 8–9). ✓
- "Record libro digerito" fields + flags → `library.digestBook`, Task 3; `store_core` packing, Task 8; `sync.c` unpack, Task 11. ✓
- "Persistent storage orologio" (`PK_BOOKS_CACHE`, `PK_BOOKS_CACHE_SHADOW`, `PK_CUR_SESSION`, `PK_SESSION_QUEUE`, `PK_SELECTED_BOOK`, `PK_SCHEMA_VERSION`) → Tasks 8–9 key map; `PK_SELECTED_BOOK` + `PK_CUR_SESSION` already in SP1. ✓
- "PERSIST_DATA_MAX_LENGTH … spezzato su più chiavi numerate" → one record per numbered key, `t_book_pack_fits_persist_limit`, Task 8. ✓
- AppMessage keys table (`SNAPSHOT_BEGIN/BOOK/SNAPSHOT_END/SESSION/SESSION_ACK/SESSION_RETRACT/RETRACT_ACK`) → `package.json` Task 11; encode/decode Tasks 11–12. ✓
- "outbox piccolo … un solo record per volta" → `sync_core` one-in-flight, `t_queue_sends_one_and_waits_for_ack`, Task 10; `sendSnapshot` step-by-step, Task 12. ✓
- "Flusso al lancio" (show cache immediately; pkjs `ready` → snapshot; shadow → commit atomico; drena la coda) → Tasks 12–13. ✓
- "Retract" (rimuovi da coda; se ACKata manda `SESSION_RETRACT`; torna a PAUSED) → `sync_core_retract`, Tasks 10, 13; SP1 already returns to PAUSED. ✓
- "Comportamento offline / BT giù" (read-only cache; queue drains later; no blocking errors; soft cap 50) → `STORE_MAX_QUEUE 50`, `queue_push` returns false when full, Task 9; retry on tick/launch, Task 13. ✓
- "Conflitti" (books only phone-side, sessions only watch-side; `current_page` rule in `library.js`) → `bookCurrentPage`, Task 2. ✓
- "Companion PebbleKit JS / datastore.js" signatures → Task 1 Interfaces match verbatim (`getBooks/saveBook/deleteBook/reorderBooks/getSessions/appendSession/updateSession/deleteSession`). ✓
- "library.js" (pagesPerHour Σ/Σ; global estimate + spegnimento; colorState; validazioni) → Tasks 2–3 with explicit tests. ✓
- "index.js" (`ready`→snapshot; `appmessage`→SESSION/RETRACT; `showConfiguration`; `webviewclosed`→diff→re-snapshot) → Tasks 7, 12. ✓
- "Config page / Handoff dati" (hash in; `pebblejs://close#` out; pkjs diff) → Tasks 4–7. ✓
- "Config page / Funzioni" (colour list, reused create/edit form, delete+cascade, favorite pin, reorder, sessions tab, HTML escaping) → Tasks 4–6 with tests. ✓
- "Limite dimensione payload" → accepted as-is, no mitigation (Global Constraints / Task 4). ✓
- "Emulatore / emu-app-config" → Task 15. ✓
- "Testing" table (host C sync logic; `library.js` node; `datastore.js` node; config page jsdom; `sync.c` mocked AppMessage; end-to-end emulator) → `sync_core` + fakes Task 10; Tasks 1–7, 12, 15. ✓
- "Distribuzione" → Task 16. ✓
- Milestones SP2-a (Tasks 1–3), SP2-b (Tasks 4–7, 14), SP2-c (Tasks 8–13, 15), SP2-d (Task 16). ✓
- "Fuori scope" (server, fetch bodies, background sync, absolute watch timestamps) → not planned; `created_at` is phone arrival time (Task 1). ✓

**2. Placeholder scan:** No "TBD"/"similar to Task N"/"add error handling". The one deliberate deferred value is `CONFIG_BASE_URL` / GitHub username, flagged as `TODO(user)` in Task 14 Step 1 because it depends on the user's account — every other step carries real code or a precise recipe. Config-page tasks give the actual script functions rather than "build a form".

**3. Type consistency:** `DigestBook` (SP1 `model.h`) reused unchanged by `store_core`, `sync_core`, `library.digestBook` output shape. `QueuedSession` defined once (Task 8 `model.h`), used by `store.c`, `sync_core`, `sync.c`, `ui_common.c`. `SyncStore` / `SyncTransport` / `SyncCore` defined once (Task 10), implemented once (Task 11). `datastore` method names identical across Tasks 1, 7, 12. `library` exports grow monotonically (Tasks 2 → 3) with no renames. AppMessage key names identical between `package.json` (Task 11), `sync.c` `MESSAGE_KEY_*` (Task 11) and the pkjs dicts (Task 12). `seed_books(int*)` signature unchanged from SP1; `seed_reload()` added (Task 13).

**Gap noted and accepted:** `store.h` gains functions across Tasks 8 and 9 whose `persist_*` bodies are only exercised end-to-end in Task 15 (the emulator), not by host unit tests — `persist_*` cannot be linked on the host. The pure serialisation and index math they depend on **are** host-tested in `store_core` (Tasks 8–9). This mirrors SP1, where `store.c` had no host tests either.
