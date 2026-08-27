# TimeReader Pebble

Reading-timer app for the Pebble Time (basalt), ported from the
M5StickC Plus version in `../m5/timereader`. C rewrite on PebbleOS.

Two sub-projects:

1. **SP1 — watchapp core** (`watchapp/`): reading timer, page entry,
   per-book stats and book list, running in the `basalt` emulator with
   hard-coded seed books. Pure logic is host-tested in `tests/c/`.
2. **SP2 — companion + BT sync** (spec only for now): a PebbleKit JS
   companion, a static config page (GitHub Pages) for managing the
   library, and Bluetooth AppMessage sync. Offline-first; a server
   migration comes later.

Specs and plans live in `docs/superpowers/`.

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

# run the host-side logic tests (no SDK needed)
cd ../tests/c
./run.sh
```
