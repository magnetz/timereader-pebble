import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { JSDOM } from 'jsdom';

const HTML = readFileSync(new URL('../../config-page/index.html', import.meta.url), 'utf8');

function load(state) {
  const hash = '#' + encodeURIComponent(JSON.stringify(state));
  const dom = new JSDOM(HTML, { url: 'https://example.org/' + hash, runScripts: 'dangerously' });
  return dom.window;
}

test('renders one row per book with its colour state', () => {
  const w = load({
    books: [
      { id: 'b1', title: 'Alpha', total_pages: 100, current_page: 100, favorite: false, order: 1 },
      { id: 'b2', title: 'Beta', total_pages: 100, current_page: 0, favorite: false, order: 2 },
    ],
    sessionsByBook: { b1: [], b2: [] }, globalPph: 20,
  });
  const rows = w.document.querySelectorAll('[data-book-row]');
  assert.equal(rows.length, 2);
  assert.equal(rows[0].getAttribute('data-color'), 'completed');
  assert.equal(rows[1].getAttribute('data-color'), 'unread');
});

test('escapeHtml neutralises < " \' in titles', () => {
  const w = load({ books: [], sessionsByBook: {}, globalPph: 0 });
  assert.equal(w.__tr.escapeHtml(`<img src=x> "q" 'a'`), '&lt;img src=x&gt; &quot;q&quot; &#39;a&#39;');
});

test('a title with markup is shown as text, not parsed', () => {
  const w = load({
    books: [{ id: 'b1', title: '<b>x</b>', total_pages: 10, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0,
  });
  const row = w.document.querySelector('[data-book-row]');
  assert.equal(row.querySelector('b'), null);
  assert.match(row.textContent, /<b>x<\/b>/);
});

test('submitForm adds a new book at the end', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 10, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0 });
  w.__tr.openForm();
  w.__tr.submitForm({ title: 'New', series: '', author: '', total_pages: 50, current_page: 3 });
  const books = w.__tr._state().books;
  assert.equal(books.length, 2);
  assert.equal(books[1].title, 'New');
  assert.equal(books[1].order, 2);
});

test('toggleFavorite pins the book above non-favourites', () => {
  const w = load({ books: [
    { id: 'b1', title: 'A', total_pages: 10, current_page: 0, favorite: false, order: 1 },
    { id: 'b2', title: 'B', total_pages: 10, current_page: 0, favorite: false, order: 2 },
  ], sessionsByBook: { b1: [], b2: [] }, globalPph: 0 });
  w.__tr.toggleFavorite('b2');
  const rows = w.document.querySelectorAll('[data-book-row]');
  assert.equal(rows[0].getAttribute('data-book-id'), 'b2');
});

test('deleteBook queues a delete for its sessions too', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 10, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [{ id: 's1', book_id: 'b1', start_page: 1, end_page: 5, created_at: 1 }] }, globalPph: 0 });
  w.__tr.deleteBook('b1');
  assert.equal(w.__tr._state().books.length, 0);
  assert.deepEqual([...w.__tr._ops().filter((o) => o.op === 'delete').map((o) => String(o.id))], ['s1']);
});

test('addSession appends an op and updates the book colour', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 50, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0 });
  w.__tr.openSessions('b1');
  w.__tr.addSession('b1', { start_page: 1, end_page: 50, duration_seconds: 3600 });
  const add = w.__tr._ops().find((o) => o.op === 'add');
  assert.equal(add.session.book_id, 'b1');
  assert.equal(add.session.pages, 50);          // inclusive: 50 - 1 + 1
  assert.match(add.session.id, /^s_/);
  const row = w.document.querySelector('[data-book-row][data-book-id="b1"]');
  assert.equal(row.getAttribute('data-color'), 'completed');
});

test('addSession rejects end < start with an inline error and no op', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 50, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [] }, globalPph: 0 });
  w.__tr.openSessions('b1');
  w.__tr.addSession('b1', { start_page: 20, end_page: 5, duration_seconds: 60 });
  assert.equal(w.__tr._ops().length, 0);
  assert.match(w.document.querySelector('#session-error').textContent, /iniziale/i);
});

test('removeSession records a delete and drops it from the list', () => {
  const w = load({ books: [{ id: 'b1', title: 'A', total_pages: 50, current_page: 0, favorite: false, order: 1 }],
    sessionsByBook: { b1: [{ id: 's1', book_id: 'b1', start_page: 1, end_page: 10, pages: 10, duration_seconds: 60, created_at: 1 }] },
    globalPph: 0 });
  w.__tr.openSessions('b1');
  w.__tr.removeSession('s1');
  const ops = w.__tr._ops();
  assert.equal(ops.length, 1);
  assert.equal(ops[0].op, 'delete');
  assert.equal(ops[0].id, 's1');
  assert.equal((w.__tr._state().sessionsByBook.b1 || []).length, 0);
});
