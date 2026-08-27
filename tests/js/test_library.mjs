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
