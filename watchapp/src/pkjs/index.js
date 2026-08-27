/* index.js — PebbleKit JS entry point.
 *
 * Lifecycle bridge between the phone (datastore.js / library.js) and the
 * watch (AppMessage). No sync in background: everything happens while the
 * watchapp is in the foreground and this script is alive.
 */
var datastore = require('./datastore');
var library = require('./library');

/* GitHub Pages serves the config-page/ folder at the repo root path. */
var CONFIG_BASE_URL = 'https://magnetz.github.io/timereader-pebble/';

/* ---- config page bridge ------------------------------------------------ */

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
    /* The config page owns order: persist each book at its list position so
       a stale `order` on the incoming object can't reshuffle the library. */
    books.forEach(function (b, i) {
      chain = chain.then(function () { return datastore.saveBook(Object.assign({}, b, { order: i + 1 })); });
    });
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

/* ---- AppMessage: snapshot out, sessions in --------------------------- */

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
    console.log('[TR] snapshot: ' + (msgs.length - 2) + ' books');
    (function step(i) {
      if (i >= msgs.length) return;
      Pebble.sendAppMessage(msgs[i], function () { step(i + 1); }, function () {
        console.log('[TR] snapshot send failed at ' + i);
        step(i + 1);
      });
    })(0);
  });
}

var api = {
  CONFIG_BASE_URL: CONFIG_BASE_URL,
  buildConfigPayload: buildConfigPayload,
  applyConfigResult: applyConfigResult,
  buildSnapshotMessages: buildSnapshotMessages,
  handleSessionMessage: handleSessionMessage,
  handleRetractMessage: handleRetractMessage,
  sendSnapshot: sendSnapshot,
};

if (typeof Pebble !== 'undefined') {
  Pebble.addEventListener('ready', function () {
    console.log('[TR] pkjs ready, sending snapshot');
    sendSnapshot();
  });

  Pebble.addEventListener('appmessage', function (e) {
    var d = e.payload || {};
    if (d.SESSION_ID !== undefined) {
      console.log('[TR] got SESSION ' + d.SESSION_ID);
      handleSessionMessage(d).then(function (ack) {
        console.log('[TR] ACK ' + ack.SESSION_ACK_ID);
        Pebble.sendAppMessage(ack);
      });
    } else if (d.SESSION_RETRACT_ID !== undefined) {
      console.log('[TR] got RETRACT ' + d.SESSION_RETRACT_ID);
      handleRetractMessage(d).then(function (ack) { Pebble.sendAppMessage(ack); });
    }
  });

  Pebble.addEventListener('showConfiguration', function () {
    buildConfigPayload().then(function (payload) {
      Pebble.openURL(CONFIG_BASE_URL + '#' + encodeURIComponent(JSON.stringify(payload)));
    });
  });

  Pebble.addEventListener('webviewclosed', function (e) {
    var raw = e && e.response ? e.response : '';
    var result = {};
    try {
      result = JSON.parse(raw);
    } catch (x) {
      try { result = JSON.parse(decodeURIComponent(raw)); } catch (y) { result = {}; }
    }
    console.log('[TR] config closed, ' + ((result.books || []).length) + ' books back');
    applyConfigResult(result).then(function () { sendSnapshot(); });
  });
}

module.exports = api;
