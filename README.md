# TimeReader Pebble

Reading-timer app for the Pebble Time (basalt), ported from the
M5StickC Plus version in `../m5/timereader`. C rewrite on PebbleOS.

**Status:** SP1 + SP2 complete. Publishing is manual — see
[`docs/distribution.md`](docs/distribution.md).

Two sub-projects:

1. **SP1 — watchapp core** (`watchapp/src/c/`): reading timer, page entry,
   per-book stats, book list, end-session menu, summary + retract,
   crash recovery. Pure logic is host-tested in `tests/c/`.
2. **SP2 — companion + BT sync** (`watchapp/src/pkjs/`, `config-page/`): a
   PebbleKit JS companion (`datastore.js` seam + `library.js` stats +
   `index.js` lifecycle), a stateless static config page for managing the
   library, and Bluetooth AppMessage sync (full snapshot in, persistent
   session queue out, idempotent ACKs, retract). Offline-first; a server
   migration comes later and touches only `datastore.js`.

Specs and plans live in `docs/superpowers/`. End-to-end sync verification:
[`docs/sp2-e2e-notes.md`](docs/sp2-e2e-notes.md). On-device checklist:
[`docs/on-device-checklist.md`](docs/on-device-checklist.md).

## Dev setup

```bash
# one-time toolchain install
curl -LsSf https://astral.sh/uv/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
uv tool install pebble-tool --python 3.13
pebble sdk install latest

# build + run the watchapp
cd watchapp
pebble build
pebble install --emulator basalt
pebble screenshot            # capture the current emulator screen

# host-side logic tests (no SDK needed)
cd ../tests/c && ./run.sh    # C: digit entry, session, state machine, store, sync core

# companion tests
cd ../js
npm i --no-save jsdom@25     # one-time
./run.sh                     # datastore, library, config page (jsdom), pkjs bridge
```

## Config page

`config-page/index.html` is a single dependency-free static file. Preview
and the `pebble emu-app-config` cycle are described in
[`config-page/README.md`](config-page/README.md). Deployed to GitHub Pages
by `.github/workflows/pages.yml`.
