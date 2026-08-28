// Host-side tests for DisplayRegisterWriteQueue (components/volcano/
// display_register_write_queue.h): no ESP-IDF, no ESPHome runtime, no real
// BLE client or hardware needed -- the header has no BLE/ESP-IDF dependency
// of its own, unlike the rest of VolcanoBleClient, so it builds directly
// against the real header with no fakes required. See test/Makefile.
//
// No external test framework, matching volcano_device_test.cpp: a handful
// of CHECK()s and a summary line is enough for this.

#include "display_register_write_queue.h"

#include <cstdio>
#include <cstdlib>

using esphome::volcano::DisplayRegisterWriteQueue;
using esphome::volcano::VolcanoField;

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

// A freshly-constructed queue has nothing tracked -- a completion arriving
// with nothing pushed must report that rather than crash or fabricate a
// field.
void test_pop_on_empty_queue_returns_nullopt() {
  DisplayRegisterWriteQueue queue;
  CHECK(!queue.pop().has_value());
}

// The single-write case: one push, one pop, matching.
void test_single_push_pops_the_same_field() {
  DisplayRegisterWriteQueue queue;
  queue.push(VolcanoField::DISPLAY_ON_COOLING);
  auto field = queue.pop();
  CHECK(field.has_value());
  CHECK(*field == VolcanoField::DISPLAY_ON_COOLING);
}

// The reason this queue exists rather than a single "most recent write"
// field: two independent writes to the same handle complete in the order
// they were sent, not in whatever order a caller might otherwise assume.
// display_on_cooling pushed first must pop first, even though
// display_units_fahrenheit was the second call made.
void test_two_independent_writes_resolve_in_fifo_order() {
  DisplayRegisterWriteQueue queue;
  queue.push(VolcanoField::DISPLAY_ON_COOLING);
  queue.push(VolcanoField::DISPLAY_UNITS_FAHRENHEIT);

  auto first = queue.pop();
  CHECK(first.has_value());
  CHECK(*first == VolcanoField::DISPLAY_ON_COOLING);

  auto second = queue.pop();
  CHECK(second.has_value());
  CHECK(*second == VolcanoField::DISPLAY_UNITS_FAHRENHEIT);
}

// A push made after the first pair has already resolved must not be
// confused with an earlier one -- the queue has no memory of what it
// already returned.
void test_queue_is_empty_again_once_drained() {
  DisplayRegisterWriteQueue queue;
  queue.push(VolcanoField::DISPLAY_ON_COOLING);
  queue.pop();
  CHECK(!queue.pop().has_value());

  queue.push(VolcanoField::DISPLAY_UNITS_FAHRENHEIT);
  auto field = queue.pop();
  CHECK(field.has_value());
  CHECK(*field == VolcanoField::DISPLAY_UNITS_FAHRENHEIT);
}

// A third write can be pushed while two earlier ones are still outstanding,
// not only after the queue has fully drained -- the case
// test_two_independent_writes_resolve_in_fifo_order and
// test_queue_is_empty_again_once_drained don't cover between them, since
// both only ever push while the queue is empty. Order must still follow
// send order throughout: the push in the middle must not jump ahead of the
// write that was already queued when it was issued.
void test_interleaved_push_and_pop_preserves_fifo_order() {
  DisplayRegisterWriteQueue queue;
  // 1st display-on-cooling write, then the 1st units write.
  queue.push(VolcanoField::DISPLAY_ON_COOLING);
  queue.push(VolcanoField::DISPLAY_UNITS_FAHRENHEIT);

  auto first = queue.pop();
  CHECK(first.has_value());
  CHECK(*first == VolcanoField::DISPLAY_ON_COOLING);

  // 2nd display-on-cooling write, issued while the units write above is
  // still outstanding.
  queue.push(VolcanoField::DISPLAY_ON_COOLING);

  // Must be the 1st units write, not the display-on-cooling write just
  // pushed.
  auto second = queue.pop();
  CHECK(second.has_value());
  CHECK(*second == VolcanoField::DISPLAY_UNITS_FAHRENHEIT);

  // The 2nd display-on-cooling write.
  auto third = queue.pop();
  CHECK(third.has_value());
  CHECK(*third == VolcanoField::DISPLAY_ON_COOLING);
}

// clear() (called on disconnect) discards anything still outstanding, so a
// future connection's first completion on this handle can never pop a
// stale entry left over from before the link dropped.
void test_clear_discards_pending_entries() {
  DisplayRegisterWriteQueue queue;
  queue.push(VolcanoField::DISPLAY_ON_COOLING);
  queue.push(VolcanoField::DISPLAY_UNITS_FAHRENHEIT);
  queue.clear();
  CHECK(!queue.pop().has_value());
}

int main() {
  test_pop_on_empty_queue_returns_nullopt();
  test_single_push_pops_the_same_field();
  test_two_independent_writes_resolve_in_fifo_order();
  test_interleaved_push_and_pop_preserves_fifo_order();
  test_queue_is_empty_again_once_drained();
  test_clear_discards_pending_entries();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
