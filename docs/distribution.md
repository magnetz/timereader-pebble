# Distribution

Pebble Time is **Bluetooth-only** — there is no USB install. Everything
goes through the Pebble / Rebble app on the phone.

## Build the release bundle

```bash
export PATH="$HOME/.local/bin:$PATH"
cd watchapp
pebble clean && pebble build
```

Artefact: `watchapp/build/watchapp.pbw`. It contains the watchapp **and**
the bundled PebbleKit JS companion (`pebble-js-app.js` = `index.js` +
`library.js` + `datastore.js`), so a single download gives the user
everything.

## Sideload (fastest, for testers)

1. Attach `watchapp/build/watchapp.pbw` to a **GitHub Release** on
   `magnetz/timereader-pebble`.
2. On the phone, open the `.pbw` link with **Sideload Helper by Rebble**
   (or the current Pebble app) → it installs to the watch over Bluetooth.
3. Or, on the same network as the phone:
   `pebble install --phone <phone-ip>` (needs Developer Connection enabled
   in the Pebble app).

## Rebble App Store (public, free)

1. Sign in at <https://apps.rebble.io/> with the developer account.
2. Create a new **watchapp** listing, UUID
   `bfdd20e5-1c38-4c97-85ee-486042b64b96`, category "Tools" / "Utilities".
3. Upload `watchapp.pbw`, add screenshots (use `pebble screenshot` from
   the emulator: list, book detail, timer, summary), a description, and
   the config-page URL as the settings page.
4. Publish. Updates are pushed by uploading a new `.pbw` with a bumped
   `version` in `package.json`.

## Config page

Lives on GitHub Pages, deployed from `config-page/` by
`.github/workflows/pages.yml` on push to `main`. Enable Pages for the repo
(Settings → Pages → Source: GitHub Actions) once. Live at
<https://magnetz.github.io/timereader-pebble/>.

The config page can be updated **without** rebuilding or re-publishing the
`.pbw`, as long as the AppMessage `messageKeys` in `package.json` don't
change. If they do change, the `.pbw` and the page must ship together.
