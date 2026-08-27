# TimeReader config page

A single static `index.html` (inline CSS/JS, no dependencies, theme-aware)
for managing the reading library from the phone. It is **stateless UI**:

- On open, PebbleKit JS passes the library in the URL hash:
  `#<encodeURIComponent(JSON.stringify({ books, sessionsByBook, globalPph }))>`.
- On "Salva", the page hands a result back via
  `pebblejs://close#<encodeURIComponent(JSON.stringify({ books, sessionOps }))>`
  and `index.js` diffs it into `datastore.js`.

## Preview locally

```bash
cd config-page
python3 -m http.server 8000
```

Then open a URL with a hand-built hash, e.g.:

```
http://localhost:8000/#%7B%22books%22%3A%5B%5D%2C%22sessionsByBook%22%3A%7B%7D%2C%22globalPph%22%3A0%7D
```

## Full round-trip in the emulator

```bash
cd ../watchapp
pebble build && pebble install --emulator basalt
pebble emu-app-config --file ../config-page/index.html
```

`emu-app-config` simulates the whole `showConfiguration` → `webviewclosed`
cycle without a phone or watch.

## Deployment

Published to GitHub Pages from `config-page/` on push to `main`
(`.github/workflows/pages.yml`). Live at
<https://magnetz.github.io/timereader-pebble/>. The page can be
updated independently of the `.pbw` as long as the AppMessage
`messageKeys` don't change.
