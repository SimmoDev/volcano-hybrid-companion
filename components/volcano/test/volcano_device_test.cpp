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

// STATE-013's silent-drop signature: the device answers a write with a
// *different* value than what was requested. requested() must still clear
// (the write is resolved, just not the way asked), and value() must land on
// what the device actually reported -- never on the stale request.
void test_silent_drop_lands_on_devices_reported_value() {
  VolcanoBleClient fake_ble;
  VolcanoDevice device;
  device.set_ble_client(&fake_ble);

  device.set_target_temperature(200.0f);
  CHECK(device.target_temperature().requested().has_value());
  CHECK(*device.target_temperature().requested() == 200.0f);

  device.on_target_temperature(150.0f);  // device reports the previous target
  CHECK(!device.target_temperature().requested().has_value());
  CHECK(device.target_temperature().is_known());
  CHECK(device.target_temperature().value() == 150.0f);
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
  test_silent_drop_lands_on_devices_reported_value();
  test_pending_write_times_out_leaving_value_untouched();
  test_write_failed_clears_requested_immediately();
  test_callback_fires_on_change_only();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
