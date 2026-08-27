/* datastore.js — the persistence seam.
 *
 * The ONLY module that knows how data is stored. Today: JSON in the
 * PebbleKit JS localStorage sandbox under `tr_books` / `tr_sessions`.
 * After the future server migration: same signatures, body swapped for
 * fetch() calls. library.js / index.js / the config page never touch
 * storage directly.
 */
var _storage = (typeof localStorage !== 'undefined') ? localStorage : null;

function _setStorage(impl) { _storage = impl; }
function _read(key) {
  try { return JSON.parse(_storage.getItem(key)) || []; } catch (e) { return []; }
}
function _write(key, arr) { _storage.setItem(key, JSON.stringify(arr)); }
function _genId(prefix) {
  var s = '';
  for (var i = 0; i < 6; i++) s += Math.floor(Math.random() * 16).toString(16);
  return prefix + s;
}

function getBooks() {
  return Promise.resolve(_read('tr_books').slice().sort(function (a, b) {
    return (a.order || 0) - (b.order || 0);
  }));
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
  var all = _read('tr_sessions').slice().sort(function (a, b) {
    return (a.created_at || 0) - (b.created_at || 0);
  });
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
