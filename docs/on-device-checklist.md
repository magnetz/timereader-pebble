# TimeReader Pebble SP1 — on-device / emulator verification checklist

Build and load first:

```bash
export PATH="$HOME/.local/bin:$PATH"
cd watchapp && pebble build && pebble install --emulator basalt
```

(`pebble wipe` between runs clears persistent storage so recovery starts clean.)

## Book list

- [ ] Two started books show their title in cyan, the unread book in
      white, a completed book (if any) in green.
- [ ] The favourite book has a `★` before the title.
- [ ] Up/Down move the highlight with the native MenuLayer animation and
      wrap around both directions.
- [ ] A very long title (e.g. "Harry Potter e il Prigioniero di Azkaban")
      wraps onto two lines inside its row without spilling into the next.
- [ ] Select opens the book detail; Back from the list exits the app.

## Book detail

- [ ] Status bar is present; three info pages cycle 0→1→2→0 with Down
      (and backwards with Up); the page-dots indicator tracks the active
      page.
- [ ] Page 0 shows the current page big and "N.N pag/ora"; "(stima)" is
      shown only for a book with no real sessions yet.
- [ ] Page 1 "Tempo tot" and "Resta … ~N min/h" do not overlap the dots.
- [ ] Page 2 shows the completion percentage and a progress bar, or
      "Pagine totali sconosciute" when the book has no page count.
- [ ] Back returns to the list.
- [ ] Opening a completed book and pressing Select does nothing — no
      session starts.

## Page entry (start / end)

- [ ] Start-page entry is prefilled with `current_page + 1` for a started
      book, or `1` for a never-started book.
- [ ] Up/Down change the active digit and wrap 9→0 and 0→9.
- [ ] Select advances the highlight to the next digit with the sliding
      ease-in-out animation; on the last digit it confirms.
- [ ] Back steps one digit left keeping the value; Back on the first
      digit cancels — to book detail from start entry, back to the timer
      (state and elapsed intact) from end entry.
- [ ] An end page lower than the start page flashes
      "Pagina finale < iniziale" for ~1.5s and stays on the entry screen.

## Timer

- [ ] Status bar present; `mm:ss` counts up once per second in the big
      bitham font; the book title is below it.
- [ ] Action bar shows the pause icon while running, the play icon while
      paused.
- [ ] Select toggles pause/resume; while paused the time is frozen and
      "PAUSA" shows in yellow.
- [ ] A session longer than an hour switches the clock to `h:mm:ss`
      without truncating.
- [ ] Back opens the end-session menu.

## End-session menu

- [ ] Back from the timer opens the native ActionMenu with three items.
- [ ] "Annulla" and a plain Back both return to the timer unchanged.
- [ ] "Salva pagina finale" opens end-page entry prefilled with the
      session's start page.
- [ ] "Esci senza salvare" asks "Uscire senza salvare?"; Select there
      returns to book detail (recovery record cleared), Back aborts back
      to the menu.

## Session summary

- [ ] After a valid end page the summary shows pages read (inclusive:
      `end - start + 1`), the duration, and that session's pages/hour.
- [ ] Select or Back confirms and returns to book detail (session final).
- [ ] Up, or a long Back, retracts: returns to the **paused** timer with
      the elapsed time it had, not advancing.

## Recovery

- [ ] Reinstall (or kill + relaunch) the app mid-RUNNING: it reopens on
      the **paused** timer at roughly the last-persisted elapsed, and the
      time does not tick until you press Select.
- [ ] With no session in progress the app opens on the book list.

## SP2 — companion + Bluetooth sync

- [ ] With the phone reachable, opening the watchapp pulls the library:
      the book list matches what the config page shows (titles, colours,
      favourite order). An empty library shows "Nessun libro".
- [ ] Editing a book / adding a session in the config page and closing it
      updates the watch on its next open (or immediately if it is open).
- [ ] Completing a session on the watch makes it appear under its book in
      the config page, with the book's current page / colour updated.
- [ ] Retract from the summary (Up / long Back) removes that session from
      the config page; returning to book detail first makes it permanent.
- [ ] Turn Bluetooth off, complete a session, turn it back on (or reopen
      the app): the queued session drains to the phone with no data loss
      and no blocking error on the watch.
- [ ] Kill the app mid-snapshot (BT drop): the previous book list is
      still shown, not a partial one.
