# Rebble App Store listing

**Published:** https://apps.rePebble.com/651226388f4f4e8989e1f1f2
(app id `651226388f4f4e8989e1f1f2`). Live at 1.0.1.

## Still to do in the Rebble dashboard (CLI can't do these)

`pebble publish` created releases 1.0.2 and 1.0.3 and uploaded 5
screenshots, but the public listing still shows 1.0.1 with 1 screenshot
and a short description. Rebble holds updates for manual promotion, and
`--description` only applies at app-creation time. In
<https://dev-portal.rebble.io/> → TimeReader:

1. **Promote release 1.0.3** to published (or whichever is latest).
2. **Replace the description** with the long version below (the app was
   created with a one-line placeholder).
3. **Confirm the 5 screenshots** are attached and ordered
   list → detail → time/remaining → timer → summary.
4. Optionally set a nicer icon (current one is `docs/store/icon-large.png`,
   a flat clock).

Future updates once the above is done: `cd watchapp && pebble clean &&
pebble build && pebble publish --release-notes "..."`, then promote in the
dashboard.

Assets live in `docs/store/`.

- **Name:** TimeReader
- **UUID:** `bfdd20e5-1c38-4c97-85ee-486042b64b96`
- **Category:** Tools & Utilities
- **Source:** https://github.com/magnetz/timereader-pebble
- **Settings page:** https://magnetz.github.io/timereader-pebble/
- **Icons:** `docs/store/icon-small.png` (48×48), `docs/store/icon-large.png` (720×720)
- **Screenshots (basalt):** `basalt_1_list.png`, `basalt_2_detail.png`,
  `basalt_3_detail_p1.png`, `basalt_5_timer.png`, `basalt_6_summary.png`

## Description

> Track how long you actually spend reading each book.
>
> TimeReader is a reading stopwatch for your wrist. Pick a book, start the
> timer when you open it, pause when life interrupts, and enter the page
> you stopped on when you're done. The watch keeps a running estimate of
> your reading speed (pages per hour, computed the honest way — total
> pages over total time), how far you have left, and roughly how long it
> will take to finish.
>
> Your library lives on your phone. Add and edit books from the settings
> page; completed reading sessions sync back from the watch automatically
> whenever the app is open. Everything works offline — no account, no
> cloud.
>
> - Native Pebble UI: MenuLayer book list, ActionBar timer, ActionMenu,
>   system transitions and fonts
> - 4-button control, Back always means "one step back"
> - Pause/resume with the elapsed time frozen; crash recovery always
>   resumes paused, never running
> - Undo a just-finished session straight from the summary screen

## Release notes (1.0.1)

> First public release. On-watch reading timer with per-book stats, a
> phone-side library manager, and Bluetooth sync of completed sessions.

## Publishing

### Option A — `pebble` CLI (for you; keeps future updates one command)

```bash
export PATH="$HOME/.local/bin:$PATH"
pebble login            # opens the browser, sign in with your Rebble account
cd watchapp
pebble build
pebble publish \
  --name "TimeReader" \
  --category "Tools & Utilities" \
  --source "https://github.com/magnetz/timereader-pebble" \
  --icon-small ../docs/store/icon-small.png \
  --icon-large ../docs/store/icon-large.png \
  --screenshots ../docs/store/basalt_1_list.png ../docs/store/basalt_2_detail.png \
                ../docs/store/basalt_3_detail_p1.png ../docs/store/basalt_5_timer.png \
                ../docs/store/basalt_6_summary.png \
  --release-notes "First public release."
```

After the app exists, every future update is just `pebble build && pebble publish`.

### Option B — web portal

<https://dev-portal.rebble.io/> → New App → watchapp → upload
`watchapp/build/watchapp.pbw`, paste the description above, set the
settings-page URL, upload the icons and screenshots → Publish.
