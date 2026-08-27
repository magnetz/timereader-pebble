import { test } from 'node:test';
import assert from 'node:assert/strict';
import ds from '../../watchapp/src/pkjs/datastore.js';
import bridge from '../../watchapp/src/pkjs/index.js';

function fakeStorage() {
  const m = new Map();
  return { getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)) };
}

test('buildConfigPayload groups sessions by book and computes globalPph', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A', total_pages: 100 });
  await ds.appendSession({ id: 's1', book_id: a.id, start_page: 1, end_page: 20, pages: 20, duration_seconds: 3600, source: 'manual' });
  const p = await bridge.buildConfigPayload();
  assert.deepEqual(Object.keys(p.sessionsByBook), [a.id]);
  assert.equal(p.globalPph, 20);
});

test('applyConfigResult persists book edits, reorder and session ops', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  const b = await ds.saveBook({ title: 'B' });
  await bridge.applyConfigResult({
    books: [{ ...b, title: 'B2' }, { ...a }],
    sessionOps: [
      { op: 'add', session: { id: 's9', book_id: a.id, start_page: 1, end_page: 9, pages: 9, duration_seconds: 60, source: 'manual' } },
    ],
  });
  const books = await ds.getBooks();
  assert.deepEqual(books.map((x) => x.title), ['B2', 'A']);   // reordered
  assert.deepEqual((await ds.getSessions()).map((s) => s.id), ['s9']);
});

test('applyConfigResult deletes books dropped from the list', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A' });
  await ds.saveBook({ title: 'B' });
  await bridge.applyConfigResult({ books: [{ ...a }], sessionOps: [] });
  assert.deepEqual((await ds.getBooks()).map((x) => x.id), [a.id]);
});

test('applyConfigResult ignores a cancelled result', async () => {
  ds._setStorage(fakeStorage());
  await ds.saveBook({ title: 'A' });
  await bridge.applyConfigResult({ cancelled: true });
  assert.equal((await ds.getBooks()).length, 1);
});

test('buildSnapshotMessages brackets the books with BEGIN/END', async () => {
  ds._setStorage(fakeStorage());
  const a = await ds.saveBook({ title: 'A', total_pages: 10 });
  await ds.appendSession({ id: 's1', book_id: a.id, start_page: 1, end_page: 10, pages: 10, duration_seconds: 3600, source: 'manual' });
  const msgs = await bridge.buildSnapshotMessages();
  assert.equal(msgs[0].SNAPSHOT_BEGIN, 1);
  assert.equal(msgs[1].BOOK_IDX, 0);
  assert.equal(msgs[1].BOOK_ID, a.id);
  assert.equal(msgs[1].BOOK_COLOR, 2);           // 10/10 -> completed
  assert.equal(msgs[2].SNAPSHOT_END, 1);
});

test('handleSessionMessage appends idempotently and acks', async () => {
  ds._setStorage(fakeStorage());
  const dict = { SESSION_ID: 's9', SESSION_BOOK_ID: 'b1', SESSION_START_PAGE: 10, SESSION_END_PAGE: 22, SESSION_DURATION_S: 600 };
  const r1 = await bridge.handleSessionMessage(dict);
  const r2 = await bridge.handleSessionMessage(dict);
  assert.equal(r1.SESSION_ACK_ID, 's9');
  assert.equal(r2.SESSION_ACK_ID, 's9');
  assert.equal((await ds.getSessions()).length, 1);
  assert.equal((await ds.getSessions())[0].pages, 13);   // inclusive 22-10+1
});

test('handleRetractMessage deletes and acks', async () => {
  ds._setStorage(fakeStorage());
  await ds.appendSession({ id: 's9', book_id: 'b1', start_page: 1, end_page: 5, pages: 5, duration_seconds: 60, source: 'watch' });
  const r = await bridge.handleRetractMessage({ SESSION_RETRACT_ID: 's9' });
  assert.equal(r.RETRACT_ACK_ID, 's9');
  assert.equal((await ds.getSessions()).length, 0);
});
