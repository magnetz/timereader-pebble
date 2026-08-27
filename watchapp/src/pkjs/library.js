/* library.js — pure reading-library logic ported from the original
 * session.py / completion_estimate.py / storage.py. No storage access:
 * every function takes plain book / session objects.
 */
function _sum(a, f) { return a.reduce(function (m, x) { return m + f(x); }, 0); }
function _lastBy(a, key) {
  return a.slice().sort(function (x, y) { return (x[key] || 0) - (y[key] || 0); }).pop();
}

function pagesPerHour(sessions) {
  var secs = _sum(sessions, function (s) { return s.duration_seconds || 0; });
  if (secs <= 0) return 0;
  return _sum(sessions, function (s) { return s.pages || 0; }) / (secs / 3600);
}
function totalHours(sessions) {
  return _sum(sessions, function (s) { return s.duration_seconds || 0; }) / 3600;
}
function bookCurrentPage(book, sessions) {
  if (sessions && sessions.length) return _lastBy(sessions, 'created_at').end_page;
  return book.current_page || 0;
}
function globalPagesPerHour(allSessions) { return pagesPerHour(allSessions || []); }
function colorState(book, sessions) {
  var cur = bookCurrentPage(book, sessions || []);
  if (book.total_pages > 0 && cur >= book.total_pages) return 'completed';
  if ((sessions && sessions.length) || (book.current_page || 0) > 0) return 'started';
  return 'unread';
}
function validateSession(s) {
  if (s.end_page < s.start_page) return 'end_page < start_page';
  if (s.duration_seconds < 0) return 'duration_seconds < 0';
  return null;
}

function bookStats(book, sessions, globalPph) {
  sessions = sessions || [];
  var estimate = sessions.length === 0;
  var pph = estimate ? (globalPph || 0) : pagesPerHour(sessions);
  var cur = bookCurrentPage(book, sessions);
  var left = Math.max(0, (book.total_pages || 0) - cur);
  return {
    totalHours: totalHours(sessions),
    pagesPerHour: pph,
    currentPage: cur,
    pagesLeft: left,
    etaMinutes: pph > 0 ? (left / pph) * 60 : 0,
    pphIsEstimate: estimate,
  };
}
function _colorCode(name) { return name === 'completed' ? 2 : name === 'started' ? 1 : 0; }
function digestBook(book, sessions, globalPph) {
  var st = bookStats(book, sessions, globalPph);
  var color = _colorCode(colorState(book, sessions));
  var flags = (st.pphIsEstimate ? 1 : 0) | (book.favorite ? 2 : 0) | (color === 2 ? 4 : 0);
  return {
    id: book.id, title: book.title || '',
    color: color, cur_page: st.currentPage, tot_pages: book.total_pages || 0,
    pph_x100: Math.round(st.pagesPerHour * 100), hours_x100: Math.round(st.totalHours * 100),
    flags: flags,
  };
}
function computeSnapshot(books, sessionsByBook) {
  sessionsByBook = sessionsByBook || {};
  var every = [];
  Object.keys(sessionsByBook).forEach(function (k) { every = every.concat(sessionsByBook[k] || []); });
  var g = globalPagesPerHour(every);
  return books.slice().sort(function (a, b) { return (a.order || 0) - (b.order || 0); })
    .map(function (b) { return digestBook(b, sessionsByBook[b.id] || [], g); });
}

module.exports = {
  pagesPerHour: pagesPerHour, totalHours: totalHours, bookCurrentPage: bookCurrentPage,
  globalPagesPerHour: globalPagesPerHour, colorState: colorState, validateSession: validateSession,
  bookStats: bookStats, digestBook: digestBook, computeSnapshot: computeSnapshot,
};
