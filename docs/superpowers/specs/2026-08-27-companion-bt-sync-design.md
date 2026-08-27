# TimeReader Pebble — SP2: Companion PebbleKit JS + sync Bluetooth

Stato: approvato per implementazione
Data: 2026-08-27
Target hardware: **basalt** (Pebble Time / Pebble Time Steel, 144×168, 64 colori)
App UUID: `bfdd20e5-1c38-4c97-85ee-486042b64b96`

## Contesto

TimeReader è un'app di tracking del tempo di lettura. Esiste già una
versione per M5StickC Plus (ESP32, MicroPython) nel repo `m5/timereader`,
con due sotto-progetti: timer on-device e AP WiFi + webapp servita dal
device.

Questo documento specifica il **porting su Pebble Time**, che è una
riscrittura in C — MicroPython non gira su PebbleOS. Il porting si
decompone come l'originale:

- **SP1** (spec separata, da scrivere): watchapp C che gira in emulatore
  `basalt`, con logica portata (state machine, digit entry, sessione,
  statistiche di visualizzazione) e UI a 4 tasti. I libri arrivano dalla
  cache di sync; in assenza di companion mostra "Nessun libro".
- **SP2** (questo documento): il companion **PebbleKit JS** (JavaScript
  incluso nel `.pbw`), la **config page** statica per gestire la
  libreria, e il **protocollo di sincronizzazione Bluetooth** fra
  orologio e telefono.

SP1 va implementato per primo: senza watchapp non c'è niente da
sincronizzare. SP2 aggiunge i libri veri e lo storico sessioni.

### Vincolo strategico: offline-first, server dopo

In futuro la gestione della libreria e lo storico dati vivranno su un
**server** (webapp + API), con migrazione dedicata. SP2 deve funzionare
**completamente offline** (dati solo sul telefono) e isolare l'accesso ai
dati dietro un modulo `datastore.js` con interfaccia asincrona, così che
la migrazione sostituisca solo quel file con chiamate `fetch()` senza
toccare il resto.

## Decisioni chiave (dal brainstorming)

- **Fonte di verità offline**: il telefono. `datastore.js` (oggi
  `localStorage` del sandbox PebbleKit JS) è il master di libri e
  storico sessioni. Nessun account, nessun server in questa fase.
- **Companion = PebbleKit JS**, non app nativa. Il companion è incluso
  nel `.pbw`: scaricando il watchapp l'utente ha tutto. Nessun sync in
  background (limite noto di pkjs) — la sincronizzazione avviene solo
  mentre il watchapp è in foreground.
- **Config page = sito statico** ospitato su GitHub Pages, singolo
  `index.html` senza dipendenze esterne. È UI stateless: riceve i dati
  nell'hash dell'URL, li restituisce alla chiusura via `pebblejs://close`.
- **Statistiche calcolate lato pkjs** (`library.js`), non
  sull'orologio. L'orologio riceve per ogni libro un record già digerito
  e non fa aritmetica sulle sessioni.
- **Sync = full snapshot + coda sessioni**. Ad ogni lancio del watchapp
  pkjs invia l'intero elenco libri; l'orologio invia le sessioni
  completate da una coda persistente, una alla volta, con ACK
  idempotente.
- **Navigazione a 4 tasti** (Up / Down / Select / Back), niente più
  overloading dei long-press dell'originale.
- **Retract sessione** circoscritto: annullabile solo dalla schermata
  Riepilogo subito dopo la conferma della pagina finale.

## Architettura del repo

```
timereader-pebble/                    # nuovo repo, fuori da m5/
  watchapp/                           # SP1 — app C (spec separata)
    src/c/
      main.c
      state_machine.c/.h
      digit_entry.c/.h
      session.c/.h
      sync.c/.h                       # SP2 lato orologio
      store.c/.h                      # persistent storage
      ui_list.c ui_detail.c ui_timer.c ui_digit.c ui_endmenu.c ui_summary.c
    src/pkjs/                         # SP2 — companion JavaScript
      index.js                       # lifecycle, AppMessage, ponte config page
      library.js                     # CRUD libri + calcolo statistiche
      datastore.js                   # SEAM: oggi localStorage, domani fetch()
    resources/
    package.json                     # UUID, capabilities, AppMessage keys
    wscript
  config-page/                        # SP2 — sito statico (GitHub Pages)
    index.html
  tests/
    c/                                # test host della logica C (gcc, no SDK)
    js/                               # test Node di library.js / datastore.js
  docs/superpowers/specs/
```

`state_machine.c`, `digit_entry.c`, `session.c` e `library.js` sono
hardware-agnostici e testabili senza SDK Pebble. Solo `main.c`, `ui_*.c`,
`sync.c` e `store.c` toccano le API PebbleOS.

## Modello dati

### Dove vive ogni pezzo

| Dato | Master | Persistenza |
|---|---|---|
| Elenco libri | Telefono | `datastore.js` (localStorage) |
| Storico sessioni completate | Telefono | `datastore.js` |
| Statistiche per libro | Derivate | Calcolate da `library.js` a ogni snapshot |
| Sessione in corso / in pausa + recovery | Orologio | `persist_*` |
| Coda sessioni completate non ACKate | Orologio | `persist_*` |
| Libro selezionato | Orologio | `persist_*` |

### Libro (`datastore.js`)

```json
{
  "id": "b1",
  "title": "Libro di prova",
  "series": "",
  "author": "",
  "total_pages": 320,
  "current_page": 0,
  "favorite": false,
  "order": 0,
  "created_at": 1756300000
}
```

`current_page` ha effetto solo finché il libro non ha sessioni proprie;
dopo, la pagina corrente deriva dall'ultima sessione (regola portata
dall'originale, calcolata in `library.js`).

### Sessione (`datastore.js`)

```json
{
  "id": "s_ab12cd",
  "book_id": "b1",
  "start_page": 12,
  "end_page": 34,
  "pages": 22,
  "duration_seconds": 1830,
  "source": "watch",
  "created_at": 1756310000
}
```

`id` stabile assegnato alla creazione (dall'orologio per le sessioni
generate lì, da `library.js` per quelle manuali). `created_at` è il
tempo del telefono al momento dell'append (l'orologio non fornisce
timestamp assoluti affidabili; per l'ordinamento va bene l'ora di
arrivo). `source` ∈ `"watch" | "manual"`.

### Record libro digerito (orologio, ricevuto via AppMessage)

Per ogni libro l'orologio riceve e memorizza in cache:

```
id, title, color_state, current_page, total_pages,
pages_per_hour (×100, intero), total_hours (×100, intero),
flags: bit0 = pph_is_estimate, bit1 = favorite, bit2 = completed
```

`color_state`: `completed` (verde) / `started` (ciano) / `unread`
(bianco), calcolato lato pkjs.

### Persistent storage orologio

| Chiave | Contenuto |
|---|---|
| `PK_SCHEMA_VERSION` | intero, per migrazioni future |
| `PK_BOOKS_CACHE` | lista record digeriti (blob) |
| `PK_BOOKS_CACHE_SHADOW` | staging per commit atomico dello snapshot |
| `PK_CUR_SESSION` | recovery sessione in corso (`{book_id, state, start_page, elapsed_seconds}`) |
| `PK_SESSION_QUEUE` | sessioni completate in attesa di ACK |
| `PK_SELECTED_BOOK` | id libro selezionato nella lista |

A inizio SP1 va verificato `PERSIST_DATA_MAX_LENGTH` sul basalt e
dimensionati di conseguenza il numero massimo di libri in cache e la
lunghezza della coda. Se un blob sfora, va spezzato su più chiavi
numerate.

## Protocollo di sincronizzazione (AppMessage)

**Vincolo di piattaforma**: AppMessage scambia dati solo mentre il
watchapp è in foreground e pkjs è vivo. Non esiste sync in background
con la soluzione pkjs. Tutta la sincronizzazione avviene nella finestra
"watchapp aperto".

### Chiavi AppMessage (dichiarate in `package.json`)

| Direzione | Messaggio | Campi |
|---|---|---|
| pkjs → watch | `SNAPSHOT_BEGIN` | `count` |
| pkjs → watch | `BOOK` | `idx`, `id`, `title`, `color`, `cur_page`, `tot_pages`, `pph_x100`, `hours_x100`, `flags` |
| pkjs → watch | `SNAPSHOT_END` | — |
| watch → pkjs | `SESSION` | `id`, `book_id`, `start_page`, `end_page`, `duration_s` |
| pkjs → watch | `SESSION_ACK` | `id` |
| watch → pkjs | `SESSION_RETRACT` | `id` |
| pkjs → watch | `RETRACT_ACK` | `id` |

`outbox` piccolo (~256 B): un solo record `BOOK` per volta, il
successivo parte solo dopo `APP_MSG_OK` del precedente (Pebble ha un
unico messaggio in volo).

### Flusso al lancio del watchapp

1. L'orologio mostra **subito** la sua cache locale (se presente) —
   nessuna schermata bianca in attesa del telefono.
2. `pkjs` sull'evento `ready`: `datastore.getBooks()` +
   `library.computeStats()` → invia `SNAPSHOT_BEGIN`, poi un `BOOK` per
   libro, poi `SNAPSHOT_END`.
3. L'orologio accumula i `BOOK` in `PK_BOOKS_CACHE_SHADOW`. Su
   `SNAPSHOT_END` fa **commit atomico**: copia shadow → cache, cancella
   shadow, ridisegna. Se il flusso si interrompe fra `BEGIN` e `END`
   (BT cade), la shadow si scarta e resta la cache buona precedente.
4. L'orologio drena `PK_SESSION_QUEUE`: manda `SESSION` per la prima
   voce, attende `SESSION_ACK` con lo stesso `id` prima di passare alla
   successiva. Timeout o assenza di ACK → ritenta al prossimo tick o al
   prossimo lancio.
5. `pkjs` su `SESSION`: `datastore.appendSession()` (idempotente per
   `id` — se già presente, non duplica) → invia `SESSION_ACK {id}`.

### Retract

Dalla schermata Riepilogo, subito dopo la conferma della pagina finale,
l'utente può annullare la sessione appena chiusa:

1. L'orologio rimuove la voce da `PK_SESSION_QUEUE` per `id`.
2. Se la voce era già stata ACKata, invia `SESSION_RETRACT {id}`;
   `pkjs` fa `datastore.deleteSession(id)` (no-op se assente) e risponde
   `RETRACT_ACK {id}`. ACK perso → l'orologio ritenta.
3. Lo stato torna a **PAUSED** con il tempo trascorso che aveva.

Il retract è possibile **solo** da quella schermata e **solo** per la
sessione appena conclusa. Una volta tornati al Dettaglio libro la
sessione è definitiva (modificabile dalla config page).

### Comportamento offline / BT giù

- Il watchapp funziona in sola lettura sulla cache.
- Le sessioni nuove si accodano in `PK_SESSION_QUEUE` e partono quando
  il telefono torna raggiungibile.
- Nessun errore bloccante nella UI.
- Coda senza cap distruttivo; tetto morbido a 50 voci con un avviso
  discreto sull'orologio se superato.

### Conflitti

Ridotti per costruzione: i libri li tocca solo il telefono, le sessioni
le genera solo l'orologio. L'unico incrocio è `current_page`, risolto
dalla regola "conta solo finché il libro non ha sessioni proprie"
(calcolo in `library.js`).

## Companion PebbleKit JS

### `datastore.js` — il seam

Interfaccia asincrona (tutte `Promise`), unico punto che conosce la
persistenza:

```
getBooks() -> Book[]
saveBook(book) -> Book            // upsert per id, assegna id/order se nuovo
deleteBook(id) -> void            // rimuove anche le sue sessioni
reorderBooks(orderedIds) -> void
getSessions(bookId?) -> Session[]
appendSession(session) -> Session // idempotente per id
updateSession(session) -> Session
deleteSession(id) -> void
```

Implementazione offline: JSON in `localStorage` sotto le chiavi
`tr_books` e `tr_sessions`. Migrazione futura: stesse firme, corpo che
fa `fetch()` verso l'API con retry/backoff. `library.js`, `index.js` e
la config page non cambiano.

### `library.js` — logica portata

Port di `session.py` / `storage.py` / `completion_estimate.py`
dell'originale:

- `pagesPerHour(sessions)` = `Σ pages / (Σ duration_seconds / 3600)` —
  **mai** media delle medie per-sessione (test esplicito).
- `bookStats(book, sessions, globalPph)`: ore totali, pag/ora,
  pagina corrente effettiva, pagine rimanenti, ETA, flag
  `pph_is_estimate`.
- Stima globale: per un libro senza sessioni proprie, `pages_per_hour`
  = media cumulativa di tutti i libri; appena il libro ha una sessione
  reale si passa al suo tasso e il flag si spegne.
- `colorState(book, sessions)`: `completed` se pagina corrente ≥
  `total_pages`, `started` se ci sono sessioni o `current_page` > 0,
  altrimenti `unread`.
- Validazioni: `end_page ≥ start_page`, `duration_seconds` ≥ 0.

### `index.js` — lifecycle e ponte

- `ready` → esegue uno snapshot verso l'orologio.
- `appmessage` → gestisce `SESSION`, `SESSION_RETRACT` (append/delete su
  `datastore`, poi ACK).
- `showConfiguration` → apre la config page (vedi sotto).
- `webviewclosed` → applica il risultato della config page a
  `datastore`; se il watchapp è aperto, rimanda uno snapshot.

## Config page (sito statico su GitHub Pages)

Singolo `index.html` con CSS/JS inline, nessuna dipendenza esterna,
theme-agnostica. È **UI stateless**.

### Handoff dati

1. `pkjs` in `showConfiguration`:
   `Pebble.openURL(BASE_URL + '#' + encodeURIComponent(JSON.stringify(payload)))`
   con `payload = { books, sessionsByBook, globalPph }`.
2. La pagina legge `location.hash`, popola la UI.
3. Alla chiusura ("Salva"):
   `document.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(result))`
   con `result` = elenco completo libri (con ordine e preferiti) +
   operazioni sessioni (`add` / `update` / `delete` con `id`).
4. `pkjs` in `webviewclosed` fa il diff e chiama i metodi `datastore`.

### Funzioni (port ridotto della webapp originale)

- Lista libri con stato colorato (completato / iniziato / da leggere).
- Form crea/modifica riusato per entrambi: titolo, serie, autore,
  pagine totali, pagina attuale; "+ Aggiungi libro" ⇄ "Salva modifiche"
  + "Annulla".
- Elimina libro (elimina anche le sue sessioni).
- Preferito (sposta in cima e ce lo tiene, come l'originale).
- Riordino su/giù.
- Scheda sessioni per libro: elenco + "+ Aggiungi sessione" +
  Modifica / Elimina per riga, con le validazioni di `library.js`
  applicate anche client-side.
- Escaping HTML per titoli contenenti `< " '` (test portato
  dall'originale).

### Limite dimensione payload

L'hash trasporta `books + sessionsByBook + globalPph`. Per l'ordine di
grandezza previsto (decine di libri, poche sessioni per libro) l'URL
regge. Se in futuro cresce troppo, la soluzione è un endpoint — che
arriva comunque con la migrazione server. Nessuna mitigazione ora.

### Emulatore

`pebble emu-app-config` apre la config page nel browser desktop e
simula l'intero ciclo `showConfiguration` → `webviewclosed` senza
telefono né watch fisico.

## Watchapp: interazione a 4 tasti (contesto SP2)

Il dettaglio completo della UI è nello spec SP1; qui si fissano solo le
interazioni che il protocollo di sync e le esigenze d'uso richiedono,
più il vincolo trasversale di aderenza alle linee guida Pebble.

### Aderenza alle linee guida UI ufficiali Pebble

Requisito: l'app deve **assomigliare per grafica e animazioni alle app
di sistema**. Si seguono le linee guida ufficiali
(<https://developer.rebble.io/guides/design-and-interaction/recommended/>,
<https://developer.rebble.io/docs/c/User_Interface/>) e si usano i
componenti nativi invece di reimplementarli. Vincoli concreti, da
dettagliare nello spec SP1:

- **Tipo app**: watchapp lanciata dal launcher (non watchface), con
  transizioni di window stack **di sistema** (push/pop): non si
  sovrascrivono.
- **Tasto Back**: sempre e solo `window_stack_pop`. Mai riassegnato.
  Le eccezioni di questo spec (Back = cifra precedente, Back = apre
  `END_SESSION_MENU`) restano "indietro di un passo" logico e sono
  segnalate a schermo.
- **Lista libri** → `MenuLayer` nativo: scroll Up/Down di sistema,
  evidenziazione e animazione di selezione standard, `ContentIndicator`
  per le frecce su/giù. Colore stato sul titolo (verde/ciano/bianco),
  cursore di selezione gestito dal MenuLayer.
- **`END_SESSION_MENU`** → `ActionMenu` nativo (breadcrumb e animazioni
  di sistema), non una lista custom.
- **Dettaglio libro / Timer / Riepilogo** → `Window` con
  `StatusBarLayer` (obbligatoria nelle schermate a lunga durata come il
  timer), layer custom per i valori. `ActionBarLayer` sul timer per
  mostrare l'icona dell'azione Select (pausa/riprendi).
- **Digit entry** → layer custom, ma le transizioni fra cifre usano
  `PropertyAnimation` con curva di sistema (`AnimationCurveEaseInOut`
  di default); nessun loop di redraw manuale (`AppTimer` sconsigliato
  per l'UI).
- **Tipografia di sistema**: `FONT_KEY_GOTHIC_28_BOLD` per i valori in
  evidenza, `FONT_KEY_GOTHIC_18` per le label (min 18), `bitham` per
  il `mm:ss` grande del timer. Nessun font custom salvo necessità.
- **Colori**: palette coerente col sistema via `GColorFromRGB`; il
  verde significa "completato", nessun rosso senza errore.
- **Icone**: `PDC` (Pebble Draw Command, vettoriali) per restare
  nitide e in stile di sistema; niente bitmap sgranate.
- **Layout**: rispetto dei margini di sistema, nessun disegno sotto la
  status bar, informazione minima per schermata.
- **Animazioni**: solo via Animation API con curve di sistema, usate
  per "guidare l'occhio" sui dati che cambiano (avanzamento cifra,
  cambio pagina dettaglio), non decorative.

Lo spec SP1 recepisce questi vincoli e ne definisce il dettaglio
visivo (wireframe delle 6 schermate, mapping componenti, durate
animazioni).

### Digit entry (`ENTER_START_PAGE` / `ENTER_END_PAGE`)

| Tasto | Azione |
|---|---|
| Up / Down | +/− sulla cifra attiva (wraparound 0–9) |
| Select | conferma cifra → avanza; sull'ultima → conferma tutto |
| Back | torna alla cifra precedente (valore conservato) |
| Back sulla 1ª cifra | esce dal digit entry (annulla) |

Annulla da `ENTER_START_PAGE` → Dettaglio libro, nessuna sessione
avviata. Annulla da `ENTER_END_PAGE` → torna al timer nello stato in
cui era (RUNNING o PAUSED), tempo trascorso intatto.

### Timer (`RUNNING` ⇄ `PAUSED`)

| Tasto | Azione |
|---|---|
| Select | pausa ⇄ riprendi |
| Back | apre `END_SESSION_MENU` |

`END_SESSION_MENU` (3 voci, Up/Down per scegliere, Select conferma,
Back = "Annulla"):

- **Salva pagina finale** → `ENTER_END_PAGE` (prefill = pagina iniziale).
- **Esci senza salvare** → seconda conferma ("Sei sicuro?") → Dettaglio
  libro; cancella `PK_CUR_SESSION`, nessuna riga in coda, nessun
  AppMessage.
- **Annulla** → torna al timer, stato invariato.

### Riepilogo sessione (`SESSION_SUMMARY`)

Non a tempo: resta finché l'utente non preme un tasto.

| Tasto | Azione |
|---|---|
| Select / Back | conferma → Dettaglio libro (sessione definitiva) |
| Up (o Back lungo) | retract della sessione appena chiusa → PAUSED, tempo intatto |

### Recovery al riavvio

Sessione persistita come `RUNNING` → riparte sempre in `PAUSED`, mai
ripresa in automatico (regola portata dall'originale). Il tempo
mostrato non deve avanzare prima di un "riprendi" esplicito.

## Error handling

| Situazione | Comportamento |
|---|---|
| Cache libri assente/corrotta sull'orologio | lista vuota → "Nessun libro — aggiungi dal telefono" |
| `SNAPSHOT` interrotto fra BEGIN ed END | scarta la shadow, tieni la cache precedente |
| `localStorage` pieno / non disponibile in pkjs | logga, non invia snapshot; l'orologio resta sulla cache. Con la migrazione server: retry/backoff nel `datastore` |
| `SESSION_ACK` / `RETRACT_ACK` perso | l'orologio ritenta al prossimo tick/lancio; pkjs re-ACKa un `id` già processato |
| Coda sessioni oltre 50 voci | avviso discreto sull'orologio, nessuna perdita dati |
| Payload config page troppo grande | accettato come limite noto in questa fase |

## Testing

| Livello | Strumento | Cosa copre |
|---|---|---|
| Logica C portata | test host gcc (no SDK) | `state_machine`: Back-per-cifra e annullo dal digit 1 in entrambi i digit entry; le 3 vie di `END_SESSION_MENU`; retract dal riepilogo con e senza sync già avvenuta; recovery RUNNING→PAUSED |
| `library.js` | `node:test` | `Σ pagine / Σ durata` (non media delle medie); stima globale e suo spegnimento; `colorState`; validazioni |
| `datastore.js` | `node:test` | upsert/delete idempotenti, cascade delete sessioni, riordino persistente |
| Config page | jsdom | render lista, escaping `< " '`, diff su salvataggio, form crea/modifica riusata |
| `sync.c` | AppMessage mockato | commit atomico snapshot, coda + ACK idempotente, ACK perso, snapshot interrotto, retract |
| End-to-end | emulatore `basalt` + `pebble emu-app-config` | lancio app → snapshot → sessione → ACK → modifica libri in config page → re-snapshot |
| Manuale on-device | checklist | recovery a metà sessione, wraparound cifre, batteria scarica = doppia vibrazione una volta, sideload effettivo |

## Distribuzione

### Da computer

```bash
uv tool install pebble-tool --python 3.13
pebble sdk install latest
cd watchapp
pebble build
pebble install --emulator basalt          # emulatore
pebble install --phone <IP-telefono>      # watch reale via app Pebble (Developer Connection)
```

`pebble emu-app-config` per provare la config page.
Artefatto: `watchapp/build/timereader.pbw`.

Pebble Time è solo-Bluetooth: non esiste install via USB, si passa
sempre per l'app sul telefono.

### Da telefono

- **Rebble App Store** (pubblicazione gratuita): l'utente cerca
  "TimeReader" nell'app Pebble/Rebble e installa; gli aggiornamenti li
  gestisce lo store.
- **Sideload**: `.pbw` su GitHub Releases → aperto sul telefono con
  **Sideload Helper by Rebble** (o l'app Pebble corrente) → installa sul
  watch.

La config page vive su GitHub Pages, aggiornabile senza ripubblicare il
`.pbw` finché non cambiano le chiavi del payload.

## Milestoni

1. **SP1-a** — watchapp in emulatore con 2 libri hard-coded: lista
   (`MenuLayer`), dettaglio, timer (`StatusBarLayer` + `ActionBarLayer`),
   digit entry (con Back-per-cifra), `END_SESSION_MENU` (`ActionMenu`),
   riepilogo con retract, recovery. Transizioni e font di sistema.
   Nessun sync.
2. **SP1-b** — persistent storage (sessione corrente + selezione);
   checklist on-device su watch reale.
3. **SP2-a** — `datastore.js` (localStorage) + `library.js` + test Node.
4. **SP2-b** — config page su GitHub Pages; ciclo `emu-app-config`
   completo.
5. **SP2-c** — `sync.c` + `index.js`: snapshot in, coda sessioni out,
   `SESSION_RETRACT`, ACK idempotenti; test su AppMessage mockato ed
   end-to-end in emulatore.
6. **SP2-d** — pubblicazione Rebble store + `.pbw` su Releases +
   istruzioni sideload.

## Fuori scope (rimandato alla migrazione server)

- Backend HTTP, account, backup cloud, sync multi-device.
- Webapp separata che sostituisce la config page.
- Sostituzione del corpo di `datastore.js` con chiamate `fetch()`.
- Sync in background (richiederebbe un companion nativo Android/iOS).
- Endpoint per payload di configurazione di grandi dimensioni.
- Timestamp assoluti generati dall'orologio (si usa l'ora di arrivo sul
  telefono).
