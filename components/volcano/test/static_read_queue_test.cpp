// Host-side tests for StaticReadQueue (components/volcano/
// static_read_queue.h): no ESP-IDF, no ESPHome runtime, no real BLE client
// or hardware needed -- the header has no BLE/ESP-IDF dependency of its own,
// unlike the rest of VolcanoBleClient, so it builds directly against the
// real header with no fakes required. See test/Makefile.
//
// No external test framework, matching volcano_device_test.cpp: a handful
// of CHECK()s and a summary line is enough for this.

#include "static_read_queue.h"

#include <cstdio>
#include <cstdlib>

using esphome::volcano::StaticReadQueue;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char *expr, const char *file, int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
  }
}

}  // namespace

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// A freshly-constructed queue has nothing to read -- done() immediately,
// current() zero -- so on_ready() fires straight away for a connection that
// resolved none of these characteristics.
void test_empty_queue_is_done() {
  StaticReadQueue queue;
  CHECK(queue.done());
  CHECK(queue.current() == 0);
}

// Handles enqueue in order and current() walks them one at a time as each
// completes.
void test_reads_are_issued_in_enqueue_order() {
  StaticReadQueue queue;
  queue.enqueue(0x0057);
  queue.enqueue(0x0052);
  queue.enqueue(0x0019);

  CHECK(!queue.done());
  CHECK(queue.current() == 0x0057);
  CHECK(queue.advance_if_current(0x0057));
  CHECK(queue.current() == 0x0052);
  CHECK(queue.advance_if_current(0x0052));
  CHECK(queue.current() == 0x0019);
  CHECK(queue.advance_if_current(0x0019));
  CHECK(queue.done());
  CHECK(queue.current() == 0);
}

// A zero handle -- a characteristic not resolved on this device -- is
// dropped at enqueue rather than queued, so the sweep never tries to read
// handle 0.
void test_zero_handles_are_not_enqueued() {
  StaticReadQueue queue;
  queue.enqueue(0x0057);
  queue.enqueue(0);
  queue.enqueue(0x0052);
  queue.enqueue(0);

  CHECK(queue.current() == 0x0057);
  CHECK(queue.advance_if_current(0x0057));
  CHECK(queue.current() == 0x0052);
  CHECK(queue.advance_if_current(0x0052));
  CHECK(queue.done());
}

// advance_if_current() only matches the slot currently due: a completion
// for a handle further down the queue, or one not in it at all, must not
// advance anything.
void test_advance_only_matches_the_current_slot() {
  StaticReadQueue queue;
  queue.enqueue(0x0057);
  queue.enqueue(0x0052);

  CHECK(!queue.advance_if_current(0x0052));  // due later, not now
  CHECK(!queue.advance_if_current(0x1234));  // not in the queue
  CHECK(queue.current() == 0x0057);          // nothing moved

  CHECK(queue.advance_if_current(0x0057));
  CHECK(queue.current() == 0x0052);
}

// Once the sweep is done, advance_if_current() returns false for any
// handle -- this is what stops a write's own read-back on a
// duration/LED-brightness handle from being credited to the (finished)
// sweep and firing on_ready() a read early.
void test_advance_after_done_is_ignored() {
  StaticReadQueue queue;
  queue.enqueue(0x0057);
  CHECK(queue.advance_if_current(0x0057));
  CHECK(queue.done());

  CHECK(!queue.advance_if_current(0x0057));  // a later read-back of the same handle
  CHECK(queue.done());
}

// skip() advances past a read that could not even be issued, so one
// unreadable characteristic does not strand the rest of the sweep.
void test_skip_advances_past_an_unissuable_read() {
  StaticReadQueue queue;
  queue.enqueue(0x0057);
  queue.enqueue(0x0052);

  queue.skip();  // 0x0057's read failed to issue
  CHECK(queue.current() == 0x0052);
  CHECK(queue.advance_if_current(0x0052));
  CHECK(queue.done());
}

// reset() discards a partial sweep -- called at the start of each
// connection's service discovery and on disconnect -- so a completion from
// a prior connection can never advance the next one's queue.
void test_reset_discards_a_partial_sweep() {
  StaticReadQueue queue;
  queue.enqueue(0x0057);
  queue.enqueue(0x0052);
  queue.advance_if_current(0x0057);

  queue.reset();
  CHECK(queue.done());
  CHECK(queue.current() == 0);

  queue.enqueue(0x0019);
  CHECK(queue.current() == 0x0019);
  CHECK(!queue.advance_if_current(0x0057));  // a stale completion from before the reset
}

// enqueue() past CAPACITY drops the overflow rather than overrunning the
// backing array. The static_assert in queue_static_reads_() is what
// actually keeps the real handle list within CAPACITY; this is the
// defensive backstop behind it.
void test_enqueue_past_capacity_is_dropped() {
  StaticReadQueue queue;
  for (uint16_t i = 0; i < StaticReadQueue::CAPACITY; i++)
    queue.enqueue(static_cast<uint16_t>(0x0010 + i));
  queue.enqueue(0xBEEF);  // one too many

  uint8_t drained = 0;
  while (!queue.done()) {
    CHECK(queue.current() != 0xBEEF);
    queue.skip();
    drained++;
  }
  CHECK(drained == StaticReadQueue::CAPACITY);
}

int main() {
  test_empty_queue_is_done();
  test_reads_are_issued_in_enqueue_order();
  test_zero_handles_are_not_enqueued();
  test_advance_only_matches_the_current_slot();
  test_advance_after_done_is_ignored();
  test_skip_advances_past_an_unissuable_read();
  test_reset_discards_a_partial_sweep();
  test_enqueue_past_capacity_is_dropped();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
