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

module.exports = {
  pagesPerHour: pagesPerHour, totalHours: totalHours, bookCurrentPage: bookCurrentPage,
  globalPagesPerHour: globalPagesPerHour, colorState: colorState, validateSession: validateSession,
};
