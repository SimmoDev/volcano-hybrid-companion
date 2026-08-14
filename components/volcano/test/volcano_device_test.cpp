// Host-side tests for VolcanoDevice (ADR-0009): no ESP-IDF, no ESPHome
// runtime, no real BLE client or hardware -- exactly the testability
// ADR-0009's Consequences section calls out as the point of the split. See
// test/Makefile for how volcano_device.cpp is compiled against the fakes
// under test/fakes/ instead of the real BLE communication layer.
//
// No external test framework: a handful of CHECK()s and a summary line is
// enough for this, and it keeps the whole suite buildable with nothing but
// a C++17 compiler.

#include "volcano_device.h"
// volcano_device.h only forward-declares VolcanoBleClient (VolcanoDevice
// holds it by pointer); tests need the full type to construct one, which
// this resolves to fakes/volcano_ble_client.h via -I order, since this file
// has no real volcano_ble_client.h next to it to take priority.
#include "volcano_ble_client.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using esphome::optional;
using esphome::volcano::ConnectionState;
using esphome::volcano::VolcanoBleClient;
using esphome::volcano::VolcanoDevice;
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

// A freshly-constructed VolcanoDevice starts disconnected with every field
// unknown -- there is no "assumed idle" resting state (ADR-0007/ADR-0009).
void test_initial_state_is_disconnected_and_unknown() {
  VolcanoDevice device;
  CHECK(device.connection_state() == ConnectionState::DISCONNECTED);
  CHECK(!device.heater().is_known());
  CHECK(!device.current_temperature().is_known());
  CHECK(!device.firmware_version().is_known());
}

// on_connecting()/on_ready() drive the three-value connection state exactly
// as ADR-0009 specifies, and on_connecting() marks live state unknown again
// -- covers the reconnect case, not just first connect.
void test_connection_lifecycle_marks_live_state_unknown_on_reconnect() {
  VolcanoDevice device;
  device.on_actuator_state(true, false);
  CHECK(device.heater().is_known());

  device.on_connecting();
  CHECK(device.connection_state() == ConnectionState::CONNECTING);
  CHECK(!device.heater().is_known());

  device.on_ready();
  CHECK(device.connection_state() == ConnectionState::READY);
}

// Device-information strings are exempt from "unknown on disconnect" --
// ADR-0009: "properties of the unit, not live state."
void test_device_info_strings_survive_disconnect() {
  VolcanoDevice device;
  device.on_firmware_version("V01.03.00.00");
  CHECK(device.firmware_version().is_known());

  device.on_disconnected();
  CHECK(device.connection_state() == ConnectionState::DISCONNECTED);
  CHECK(device.firmware_version().is_known());
  CHECK(device.firmware_version().value() == "V01.03.00.00");
}

// Everything else goes unknown on disconnect -- the general case
// test_device_info_strings_survive_disconnect is the documented exception to.
void test_live_state_goes_unknown_on_disconnect() {
  VolcanoDevice device;
  device.on_actuator_state(true, true);
  device.on_current_temperature(true, 180.0f);
  CHECK(device.heater().is_known());
  CHECK(device.pump().is_known());
  CHECK(device.current_temperature().is_known());

  device.on_disconnected();
  CHECK(!device.heater().is_known());
  CHECK(!device.pump().is_known());
  CHECK(!device.current_temperature().is_known());
}

// STATE-012: a has_reading=false report must clear current temperature to
// unknown, never surface as a 0 degC reading.
void test_current_temperature_no_reading_gate() {
  VolcanoDevice device;
  device.on_current_temperature(true, 32.0f);
  CHECK(device.current_temperature().is_known());
  CHECK(device.current_temperature().value() == 32.0f);

  device.on_current_temperature(false, 0.0f);
  CHECK(!device.current_temperature().is_known());
}

// ADR-0002: a command is a request, never an immediate state change. With
// no BLE client attached at all, set_*() must be a safe no-op.
void test_command_without_ble_client_is_safe_noop() {
  VolcanoDevice device;
  device.set_heater(true);
  CHECK(!device.heater().requested().has_value());
}

// VolcanoBleClient::write_*() returning false (out-of-range, or not
// connected) means VolcanoDevice must not record a pending request at all.
void test_command_is_noop_when_write_refused() {
  VolcanoBleClient fake_ble;
  fake_ble.next_write_result = false;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_heater(true);
  CHECK(fake_ble.last_heater == true);  // the write was still attempted
  CHECK(!device.heater().requested().has_value());
  CHECK(!device.heater().is_known());
}

// The plain-confirm path: a command sets requested(), and a matching device
// report resolves it and updates value().
void test_write_confirmed_by_matching_report() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_heater(true);
  CHECK(fake_ble.last_heater == true);
  CHECK(device.heater().requested().has_value());
  CHECK(*device.heater().requested() == true);

  device.on_actuator_state(true, false);
  CHECK(!device.heater().requested().has_value());
  CHECK(device.heater().is_known());
  CHECK(device.heater().value() == true);
}

// The same plain-confirm path as test_write_confirmed_by_matching_report,
// for pump rather than heater -- both resolve from the one on_actuator_state()
// report (STATE-008), so this also covers that the report can confirm one
// field's request while simultaneously just updating the other's value().
void test_pump_write_confirmed_by_matching_report() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_pump(true);
  CHECK(fake_ble.last_pump == true);
  CHECK(device.pump().requested().has_value());
  CHECK(*device.pump().requested() == true);

  device.on_actuator_state(false, true);
  CHECK(!device.pump().requested().has_value());
  CHECK(device.pump().is_known());
  CHECK(device.pump().value() == true);
  CHECK(device.heater().is_known());  // the same report also settles heater, just with no request to resolve
  CHECK(device.heater().value() == false);
}

// STATE-013's silent-drop signature: the device answers a write with a
// *different* value than what was requested, after that write's own
// ATT-level completion has been observed (on_write_acked()) -- so this
// report can actually be that write's outcome. requested() must still clear
// (the write is resolved, just not the way asked), and value() must land on
// what the device actually reported -- never on the stale request. This is
// the read-back path (from_read=true): nothing else ever reads this
// characteristic, so a read-sourced report is deterministically about this
// write.
void test_read_sourced_silent_drop_lands_on_devices_reported_value() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_target_temperature(200.0f);
  CHECK(device.target_temperature().requested().has_value());
  CHECK(*device.target_temperature().requested() == 200.0f);

  device.on_write_acked(VolcanoField::TARGET_TEMPERATURE);  // the write itself reached the device
  device.on_target_temperature(150.0f, true);               // the write's own read-back, previous target
  CHECK(!device.target_temperature().requested().has_value());
  CHECK(device.target_temperature().is_known());
  CHECK(device.target_temperature().value() == 150.0f);
}

// The read-sourced resolution above must not extend to a notification: a
// mismatching *notification* (from_read=false) after ack must NOT resolve
// requested(), even though it looks identical to the silent-drop signature
// above -- it could equally be a genuine, unrelated panel change (STATE-013),
// and the write's own read-back -- always issued regardless -- is what
// actually confirms this write, not this notification. value() still adopts
// the reported figure either way.
void test_notify_sourced_mismatch_does_not_resolve_target_temperature() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_target_temperature(200.0f);
  device.on_write_acked(VolcanoField::TARGET_TEMPERATURE);

  device.on_target_temperature(150.0f, false);  // a panel-originated notification, not this write's read-back
  CHECK(device.target_temperature().requested().has_value());  // still outstanding
  CHECK(*device.target_temperature().requested() == 200.0f);
  CHECK(device.target_temperature().value() == 150.0f);  // reported value still adopted

  device.on_target_temperature(200.0f, true);  // the write's own read-back, arriving after
  CHECK(!device.target_temperature().requested().has_value());
  CHECK(device.target_temperature().value() == 200.0f);
}

// The false-positive this whole mechanism exists to avoid: a mismatching
// report for a field arrives *before* that field's write has even reached
// the device (no on_write_acked() yet) -- e.g. STATE-009's temperature-
// change pulse notifying the display register while an unrelated write to
// it is still in flight. This must NOT be treated as a silent drop:
// requested() stays outstanding (the real confirmation is still to come),
// though value() still adopts the reported figure, since it may be a
// legitimate unrelated change (e.g. a panel action).
void test_unacked_mismatch_does_not_resolve_request() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_target_temperature(200.0f);
  CHECK(device.target_temperature().requested().has_value());

  device.on_target_temperature(150.0f, true);  // arrives before on_write_acked() -- not this write's outcome
  CHECK(device.target_temperature().requested().has_value());  // still outstanding
  CHECK(*device.target_temperature().requested() == 200.0f);
  CHECK(device.target_temperature().value() == 150.0f);  // reported value still adopted

  // The real confirmation, once the write actually reaches the device.
  device.on_write_acked(VolcanoField::TARGET_TEMPERATURE);
  device.on_target_temperature(200.0f, true);
  CHECK(!device.target_temperature().requested().has_value());
  CHECK(device.target_temperature().value() == 200.0f);
}

// A matching report resolves the request immediately regardless of
// on_write_acked() -- a match is never ambiguous, so it would be wrong to
// hold it pending on an ack signal that only guards against a false
// mismatch-triggered resolution.
void test_matching_report_resolves_without_ack() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_led_brightness(75);
  CHECK(device.led_brightness().requested().has_value());

  device.on_led_brightness(75);  // matches, arrives before any on_write_acked()
  CHECK(!device.led_brightness().requested().has_value());
  CHECK(device.led_brightness().value() == 75);
}

// Notification-only fields (heater, pump, vibration, display-on-cooling,
// display-units) have no read-back and no unambiguous per-write confirmation
// channel: every report is a notification, which can be triggered by a bit
// unrelated to the field being resolved -- e.g. STATE-008's vibration-alert
// pulse sharing the status register with the pump bit. Unlike target
// temperature (which has protocol evidence of a silent-drop-with-unchanged-
// echo behaviour, STATE-013), nothing in docs/protocol/ shows these fields
// ever behaving that way. A mismatch after ack must therefore never resolve
// requested() on its own for these fields -- only a later match does.
void test_notify_sourced_mismatch_does_not_resolve_pending_write() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_pump(true);
  CHECK(device.pump().requested().has_value());

  device.on_write_acked(VolcanoField::PUMP);  // the trigger write itself reached the device
  // An unrelated status-register notification -- e.g. a vibration-alert
  // pulse on bit 13 -- reporting the pump as still off, before the pump's
  // own state change has actually propagated to the register.
  device.on_actuator_state(false, false);
  CHECK(device.pump().requested().has_value());  // must still be outstanding, not treated as a drop
  CHECK(*device.pump().requested() == true);
  CHECK(device.pump().value() == false);  // reported value still adopted

  // The real confirmation, moments later.
  device.on_actuator_state(false, true);
  CHECK(!device.pump().requested().has_value());
  CHECK(device.pump().value() == true);
}

// The same field as above, but the pending write never actually confirms --
// only mismatching or absent reports arrive. Since a mismatch cannot resolve
// it, the 5s timeout in process() is what eventually does, exactly as it
// would for a field that never notified at all -- the backstop that keeps a
// genuine drop a bounded, distinct outcome rather than an indefinite hang,
// even though a mismatch alone cannot signal it immediately for this field.
void test_notify_sourced_field_falls_back_to_timeout() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);
  device.on_vibration(false);  // an earlier, already-confirmed value

  uint32_t fake_now = 1000;
  device.set_clock([&fake_now]() { return fake_now; });

  device.set_vibration(true);
  device.on_write_acked(VolcanoField::VIBRATION);
  device.on_vibration(false);  // an unrelated/premature report, must not resolve it
  CHECK(device.vibration().requested().has_value());

  fake_now += VolcanoDevice::PENDING_WRITE_TIMEOUT_MS;
  device.process();
  CHECK(!device.vibration().requested().has_value());
  CHECK(device.vibration().value() == false);  // untouched by the timed-out request
}

// A second command for a field with a request already outstanding must be
// refused, not overwrite the first request -- otherwise the first write's
// eventual read-back/notification would resolve whichever request happens
// to be current when it arrives, rather than the one it actually answers.
void test_overlapping_write_to_same_field_is_refused() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_heater(true);
  CHECK(fake_ble.last_heater == true);
  CHECK(*device.heater().requested() == true);

  device.set_heater(false);                     // refused: the first request is still outstanding
  CHECK(fake_ble.last_heater == true);          // no second write was issued (still the first write's value)
  CHECK(*device.heater().requested() == true);  // first request untouched

  device.on_write_acked(VolcanoField::HEATER);
  device.on_actuator_state(true, false);
  CHECK(!device.heater().requested().has_value());

  device.set_heater(false);  // now accepted, the previous request having resolved
  CHECK(fake_ble.last_heater == false);
  CHECK(*device.heater().requested() == false);
}

// A write that never confirms must time out after PENDING_WRITE_TIMEOUT_MS,
// clearing requested() and leaving value() exactly as it was before the
// request -- not reverting to some default, not adopting the requested value.
void test_pending_write_times_out_leaving_value_untouched() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);
  device.on_led_brightness(30);  // an earlier, already-confirmed value

  uint32_t fake_now = 1000;
  device.set_clock([&fake_now]() { return fake_now; });

  device.set_led_brightness(50);
  CHECK(device.led_brightness().requested().has_value());
  CHECK(*device.led_brightness().requested() == 50);

  fake_now += VolcanoDevice::PENDING_WRITE_TIMEOUT_MS - 1;
  device.process();
  CHECK(device.led_brightness().requested().has_value());  // not yet due

  fake_now += 1;
  device.process();
  CHECK(!device.led_brightness().requested().has_value());
  CHECK(device.led_brightness().is_known());
  CHECK(device.led_brightness().value() == 30);  // untouched by the timed-out request
}

// An immediate GATT-level write failure resolves the pending request the
// same way a timeout does -- clears requested(), leaves value() untouched --
// but without waiting for the deadline.
void test_write_failed_clears_requested_immediately() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_vibration(true);
  CHECK(device.vibration().requested().has_value());

  device.on_write_failed(VolcanoField::VIBRATION);
  CHECK(!device.vibration().requested().has_value());
  CHECK(!device.vibration().is_known());
}

// The same immediate-failure path as test_write_failed_clears_requested_immediately,
// for the two read-back-confirmed fields (LED brightness, auto-shutoff
// duration) rather than a notification-confirmed one (vibration) -- both
// confirmation sources funnel a failure through the same
// clear_requested_for_field_(), but nothing before this exercised either of
// these two specifically.
void test_write_failed_clears_requested_for_read_back_confirmed_fields() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_led_brightness(75);
  CHECK(device.led_brightness().requested().has_value());
  device.on_write_failed(VolcanoField::LED_BRIGHTNESS);
  CHECK(!device.led_brightness().requested().has_value());
  CHECK(!device.led_brightness().is_known());

  device.set_auto_shutoff_duration(600);
  CHECK(device.auto_shutoff_duration().requested().has_value());
  device.on_write_failed(VolcanoField::AUTO_SHUTOFF_DURATION);
  CHECK(!device.auto_shutoff_duration().requested().has_value());
  CHECK(!device.auto_shutoff_duration().is_known());
}

// A command in flight when the link drops must not survive the disconnect:
// on_connecting()/on_disconnected() clear requested() the same as a timeout
// or an explicit write failure would, rather than leaving it pending against
// a connection that no longer exists.
void test_pending_write_cleared_by_disconnect() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_heater(true);
  CHECK(device.heater().requested().has_value());

  device.on_disconnected();
  CHECK(!device.heater().requested().has_value());
  CHECK(!device.heater().is_known());
}

// The same clearing happens on a reconnect's on_connecting(), not only on
// on_disconnected() -- both funnel through mark_live_state_unknown_().
void test_pending_write_cleared_by_reconnect() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_target_temperature(200.0f);
  CHECK(device.target_temperature().requested().has_value());

  device.on_connecting();
  CHECK(!device.target_temperature().requested().has_value());
  CHECK(!device.target_temperature().is_known());
}

// The state-change callback fires on an actual change and does not fire
// again for a repeat report of the same value.
void test_callback_fires_on_change_only() {
  VolcanoDevice device;
  int fires = 0;
  device.add_on_state_callback([&fires]() { fires++; });

  device.on_actuator_state(true, false);
  CHECK(fires > 0);
  int fires_after_first_change = fires;

  device.on_actuator_state(true, false);  // identical report
  CHECK(fires == fires_after_first_change);

  device.on_actuator_state(false, false);  // heater actually changes
  CHECK(fires > fires_after_first_change);
}

int main() {
  test_initial_state_is_disconnected_and_unknown();
  test_connection_lifecycle_marks_live_state_unknown_on_reconnect();
  test_device_info_strings_survive_disconnect();
  test_live_state_goes_unknown_on_disconnect();
  test_current_temperature_no_reading_gate();
  test_command_without_ble_client_is_safe_noop();
  test_command_is_noop_when_write_refused();
  test_write_confirmed_by_matching_report();
  test_pump_write_confirmed_by_matching_report();
  test_read_sourced_silent_drop_lands_on_devices_reported_value();
  test_notify_sourced_mismatch_does_not_resolve_target_temperature();
  test_unacked_mismatch_does_not_resolve_request();
  test_matching_report_resolves_without_ack();
  test_notify_sourced_mismatch_does_not_resolve_pending_write();
  test_notify_sourced_field_falls_back_to_timeout();
  test_overlapping_write_to_same_field_is_refused();
  test_pending_write_times_out_leaving_value_untouched();
  test_write_failed_clears_requested_immediately();
  test_write_failed_clears_requested_for_read_back_confirmed_fields();
  test_pending_write_cleared_by_disconnect();
  test_pending_write_cleared_by_reconnect();
  test_callback_fires_on_change_only();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
