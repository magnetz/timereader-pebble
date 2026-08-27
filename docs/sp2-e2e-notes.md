# SP2 end-to-end verification (basalt emulator)

All three sync paths were exercised on the `basalt` emulator with the
PebbleKit JS companion live. `pebble` needs `export PATH="$HOME/.local/bin:$PATH"`.

## Setup

```bash
cd watchapp
pebble build && pebble install --emulator basalt
```

The phone side (pypkjs) keeps its `localStorage` at
`~/Library/Application Support/Pebble SDK/4.33.1/basalt/localstorage/<uuid>.{dir,dat}`.
For a scripted test without the config-page browser cycle you can seed it
directly (Python `dbm`) with `tr_books` / `tr_sessions` JSON arrays, then
relaunch the app so pkjs `ready` fires a fresh snapshot.

## 1. Snapshot in (phone → watch)

- Seeded `tr_books` with two books ("Il Signore degli Anelli" favourite /
  started, "1984" unread).
- On launch: `pkjs> [TR] pkjs ready, sending snapshot` → `[TR] snapshot: 2 books`.
- Watch routed `APP_NO_BOOKS → APP_LIST_BOOKS` and drew both rows with the
  right title colours and the favourite star. (`/tmp/e2e_list.png`)
- With an empty `tr_books` the log shows `snapshot: 0 books` and the watch
  stays on "Nessun libro — aggiungi dal telefono".

## 2. Session out (watch → phone) with idempotent ACK

- Started a session on the watch, saved an end page, landed on the summary
  ("Sessione salvata").
- `pkjs> [TR] got SESSION w1` → `[TR] ACK w1`.
- `tr_sessions` gained `{source:"watch", id:"w1", book_id:"b_test1", …}`.
- Relaunching the app: pkjs sends the snapshot again but does **not**
  re-receive `SESSION w1` — the watch queue was drained on ACK.

## 3. Retract (watch → phone)

- Ran another session (`w2`), `SESSION w2` → `ACK w2`.
- Pressed Up on the summary → `pkjs> [TR] got RETRACT w2`.
- `tr_sessions` back to just `w1` — the retracted row was appended then
  deleted on the phone. The watch landed on the paused timer.

## 4. Interrupted snapshot

- `sync_core` unit tests cover this (`t_interrupted_snapshot_keeps_old_cache`):
  a `SNAPSHOT_BEGIN` with no matching `SNAPSHOT_END` (BT drop / app exit)
  discards the shadow and leaves the previous cache intact. On the
  emulator, `pebble kill` mid-stream then relaunch shows the last good
  book list unchanged.

## Config page cycle

`pebble emu-app-config --emulator basalt --file ../config-page/index.html`
opens the page in the browser with the current library in the URL hash;
"Salva" returns via `pebblejs://close#…`, `index.js` `webviewclosed`
diffs it into `datastore.js` and re-sends the snapshot. Once GitHub Pages
is live the no-`--file` form uses `CONFIG_BASE_URL` instead.
