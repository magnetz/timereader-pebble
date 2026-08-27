import { test } from 'node:test';
import assert from 'node:assert/strict';
import lib from '../../watchapp/src/pkjs/library.js';

test('pagesPerHour is sum/sum, not the mean of per-session rates', () => {
  // A: 90 pages / 1h, B: 10 pages / 3h -> 100 / 4h = 25.0
  // mean of per-session rates would be (90 + 3.33)/2 = 46.67
  const ss = [
    { pages: 90, duration_seconds: 3600 },
    { pages: 10, duration_seconds: 10800 },
  ];
  assert.equal(Math.round(lib.pagesPerHour(ss) * 100), 2500);
  assert.equal(lib.pagesPerHour([]), 0);
});

test('bookCurrentPage prefers the last session end_page', () => {
  const book = { current_page: 5, total_pages: 100 };
  assert.equal(lib.bookCurrentPage(book, []), 5);
  assert.equal(lib.bookCurrentPage(book, [
    { end_page: 40, created_at: 1 }, { end_page: 73, created_at: 2 },
  ]), 73);
});

test('colorState', () => {
  assert.equal(lib.colorState({ current_page: 0, total_pages: 300 }, []), 'unread');
  assert.equal(lib.colorState({ current_page: 12, total_pages: 300 }, []), 'started');
  assert.equal(lib.colorState({ current_page: 0, total_pages: 300 },
    [{ end_page: 40, created_at: 1 }]), 'started');
  assert.equal(lib.colorState({ current_page: 0, total_pages: 300 },
    [{ end_page: 300, created_at: 1 }]), 'completed');
});

test('validateSession', () => {
  assert.equal(lib.validateSession({ start_page: 10, end_page: 10, duration_seconds: 0 }), null);
  assert.equal(lib.validateSession({ start_page: 10, end_page: 9, duration_seconds: 5 }), 'end_page < start_page');
  assert.equal(lib.validateSession({ start_page: 1, end_page: 5, duration_seconds: -1 }), 'duration_seconds < 0');
});

test('bookStats falls back to the global rate until the book has a session', () => {
  const book = { total_pages: 200, current_page: 20, favorite: false };
  const withNone = lib.bookStats(book, [], 30);
  assert.equal(withNone.pphIsEstimate, true);
  assert.equal(withNone.pagesPerHour, 30);
  assert.equal(withNone.currentPage, 20);
  assert.equal(withNone.pagesLeft, 180);
  assert.equal(Math.round(withNone.etaMinutes), 360);

  const withOne = lib.bookStats(book, [{ pages: 40, duration_seconds: 3600, end_page: 60, created_at: 1 }], 30);
  assert.equal(withOne.pphIsEstimate, false);
  assert.equal(withOne.pagesPerHour, 40);
  assert.equal(withOne.currentPage, 60);
});

test('digestBook packs flags and x100 ints', () => {
  const d = lib.digestBook(
    { id: 'b1', title: 'T', total_pages: 100, current_page: 0, favorite: true },
    [{ pages: 50, duration_seconds: 3600, end_page: 100, created_at: 1 }], 10);
  assert.equal(d.color, 2);           // completed
  assert.equal(d.pph_x100, 5000);
  assert.equal(d.hours_x100, 100);
  assert.equal(d.flags, 0 | 2 | 4);   // not estimate, favorite, completed
});

test('computeSnapshot returns records in book order with one global rate', () => {
  const books = [
    { id: 'b2', title: 'Two', order: 2, total_pages: 100, current_page: 0, favorite: false },
    { id: 'b1', title: 'One', order: 1, total_pages: 100, current_page: 0, favorite: false },
  ];
  const snap = lib.computeSnapshot(books, {
    b1: [{ pages: 20, duration_seconds: 3600, end_page: 20, created_at: 1 }],
    b2: [],
  });
  assert.deepEqual(snap.map((d) => d.id), ['b1', 'b2']);
  assert.equal(snap[1].flags & 1, 1); // b2 has no sessions -> estimate flag
});
