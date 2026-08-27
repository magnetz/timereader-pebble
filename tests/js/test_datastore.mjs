import { test } from 'node:test';
import assert from 'node:assert/strict';
import ds from '../../watchapp/src/pkjs/datastore.js';

function fakeStorage() {
  const m = new Map();
  return { getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)) };
}

test('saveBook assigns id and order, getBooks sorts by order', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A', total_pages: 100 });
  const b = await ds.saveBook({ title: 'B', total_pages: 200 });
  assert.match(a.id, /^b_[0-9a-f]{6}$/);
  assert.equal(a.order, 1);
  assert.equal(b.order, 2);
  const books = await ds.getBooks();
  assert.deepEqual(books.map((x) => x.title), ['A', 'B']);
});

test('saveBook upserts by id', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  await ds.saveBook({ ...a, title: 'A2' });
  const books = await ds.getBooks();
  assert.equal(books.length, 1);
  assert.equal(books[0].title, 'A2');
});

test('appendSession is idempotent by id', async () => {
  ds._setStorage(fakeStorage());
  const s = { id: 's_x', book_id: 'b_1', start_page: 1, end_page: 10, pages: 10, duration_seconds: 60, source: 'watch' };
  await ds.appendSession(s);
  await ds.appendSession(s);
  const all = await ds.getSessions();
  assert.equal(all.length, 1);
  assert.ok(all[0].created_at > 0);
});

test('deleteBook cascades to its sessions', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  await ds.appendSession({ id: 's1', book_id: a.id, start_page: 1, end_page: 5, pages: 5, duration_seconds: 60, source: 'manual' });
  await ds.appendSession({ id: 's2', book_id: b.id, start_page: 1, end_page: 5, pages: 5, duration_seconds: 60, source: 'manual' });
  await ds.deleteBook(a.id);
  assert.deepEqual((await ds.getBooks()).map((x) => x.id), [b.id]);
  assert.deepEqual((await ds.getSessions()).map((x) => x.id), ['s2']);
});

test('reorderBooks rewrites .order to index position', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  const c = await ds.saveBook({ title: 'C' });
  await ds.reorderBooks([c.id, a.id, b.id]);
  assert.deepEqual((await ds.getBooks()).map((x) => x.title), ['C', 'A', 'B']);
});

test('updateSession throws when the id is absent', async () => {
  ds._setStorage(fakeStorage());
  await assert.rejects(() => ds.updateSession({ id: 'nope' }));
});
