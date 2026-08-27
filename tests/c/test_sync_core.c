#include "sync_core.h"
#include "fake_sync_env.h"
#include "test.h"
#include <string.h>

static QueuedSession mk(const char *id) {
  QueuedSession s; memset(&s, 0, sizeof(s));
  strcpy(s.id, id); strcpy(s.book_id, "b1");
  s.start_page = 10; s.end_page = 20; s.duration_seconds = 600;
  return s;
}

void t_snapshot_commits_atomically(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  DigestBook b; memset(&b, 0, sizeof(b)); strcpy(b.id, "b1"); strcpy(b.title, "One");
  sync_core_snapshot_begin(&c, 1);
  sync_core_snapshot_book(&c, 0, &b);
  CHECK_EQ_INT(e.cache_count, 0, "not visible mid-snapshot");
  sync_core_snapshot_end(&c);
  CHECK_EQ_INT(e.cache_count, 1, "committed on END");
  CHECK_EQ_INT(e.on_books_changed_calls, 1, "UI told once");
}

void t_interrupted_snapshot_keeps_old_cache(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  fake_env_seed_cache(&e, 2);
  sync_core_snapshot_begin(&c, 5);
  DigestBook b; memset(&b, 0, sizeof(b)); strcpy(b.id, "x");
  sync_core_snapshot_book(&c, 0, &b);
  sync_core_snapshot_abort(&c);
  CHECK_EQ_INT(e.cache_count, 2, "old cache intact");
  CHECK_EQ_INT(e.shadow_count, 0, "shadow discarded");
}

void t_queue_sends_one_and_waits_for_ack(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  QueuedSession s1 = mk("s1"), s2 = mk("s2");
  sync_core_enqueue(&c, &s1);
  sync_core_enqueue(&c, &s2);
  CHECK_EQ_INT(e.sent_count, 1, "only the head is in flight");
  CHECK_EQ_STR(e.sent[0].id, "s1", "");
  sync_core_on_send_result(&c, true);
  sync_core_on_session_ack(&c, "s1");
  CHECK_EQ_INT(e.sent_count, 2, "next flows after ACK");
  CHECK_EQ_STR(e.sent[1].id, "s2", "");
}

void t_ack_is_idempotent(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  QueuedSession s1 = mk("s1");
  sync_core_enqueue(&c, &s1);
  sync_core_on_send_result(&c, true);
  sync_core_on_session_ack(&c, "s1");
  sync_core_on_session_ack(&c, "s1");
  CHECK_EQ_INT(e.queue_count, 0, "still empty, no crash");
}

void t_lost_ack_retries_on_next_drain(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  QueuedSession s1 = mk("s1");
  sync_core_enqueue(&c, &s1);
  sync_core_on_send_result(&c, true);
  sync_core_on_send_result(&c, false);
  sync_core_drain(&c);
  CHECK_EQ_INT(e.sent_count, 2, "re-sent s1");
  CHECK_EQ_STR(e.sent[1].id, "s1", "");
}

void t_retract_before_ack_just_drops_locally(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  QueuedSession s1 = mk("s1");
  sync_core_enqueue(&c, &s1);
  sync_core_retract(&c, "s1");
  CHECK_EQ_INT(e.queue_count, 0, "");
  CHECK_EQ_INT(e.sent_retract_count, 0, "phone never saw it");
}

void t_retract_after_ack_sends_retract_message(void) {
  FakeEnv e; fake_env_init(&e);
  SyncCore c; fake_env_wire(&e, &c);
  QueuedSession s1 = mk("s1");
  sync_core_enqueue(&c, &s1);
  sync_core_on_send_result(&c, true);
  sync_core_on_session_ack(&c, "s1");
  sync_core_retract(&c, "s1");
  CHECK_EQ_INT(e.sent_retract_count, 1, "");
  CHECK_EQ_STR(e.sent_retracts[0], "s1", "");
  sync_core_on_retract_ack(&c, "s1");
  CHECK_EQ_INT(c.pending_retract[0], 0, "cleared on RETRACT_ACK");
}

TEST_BEGIN()
  t_snapshot_commits_atomically();
  t_interrupted_snapshot_keeps_old_cache();
  t_queue_sends_one_and_waits_for_ack();
  t_ack_is_idempotent();
  t_lost_ack_retries_on_next_drain();
  t_retract_before_ack_just_drops_locally();
  t_retract_after_ack_sends_retract_message();
TEST_END()
