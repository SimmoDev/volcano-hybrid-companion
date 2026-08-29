// Host-side tests for wire_format.h (components/volcano/wire_format.h): no
// ESP-IDF, no ESPHome runtime, no real BLE client or hardware needed -- the
// header has no BLE/ESP-IDF dependency of its own, unlike the rest of
// VolcanoBleClient, so it builds directly against the real header with no
// fakes required. See test/Makefile.
//
// No external test framework, matching volcano_device_test.cpp: a handful of
// CHECK()s and a summary line is enough for this.
//
// The values asserted below are the raw bytes docs/protocol/ records as
// Confirmed, so a change that inverts a bit polarity, moves a mask or
// loosens a range fails here rather than only on real hardware.

#include "wire_format.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace esphome::volcano;

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

// CMD-001: 40.0-230.0 degC (400-2300 decidegrees) is the only Confirmed
// range; the check operates on the signed decidegrees so a negative input
// (from a nonsensical Celsius value) is refused, not wrapped.
void test_target_temperature_range() {
  CHECK(target_temperature_decidegrees_in_range(400));
  CHECK(target_temperature_decidegrees_in_range(2300));
  CHECK(target_temperature_decidegrees_in_range(1850));
  CHECK(!target_temperature_decidegrees_in_range(399));
  CHECK(!target_temperature_decidegrees_in_range(2301));
  CHECK(!target_temperature_decidegrees_in_range(0));
  CHECK(!target_temperature_decidegrees_in_range(-100));
}

// CMD-003: 60-21600 s Confirmed. The app's own floor is 1800; the lower
// bound here is deliberately below that, per the finding.
void test_auto_shutoff_duration_range() {
  CHECK(auto_shutoff_duration_seconds_in_range(60));
  CHECK(auto_shutoff_duration_seconds_in_range(21600));
  CHECK(auto_shutoff_duration_seconds_in_range(1800));
  CHECK(!auto_shutoff_duration_seconds_in_range(59));
  CHECK(!auto_shutoff_duration_seconds_in_range(0));
  CHECK(!auto_shutoff_duration_seconds_in_range(21601));
}

// CMD-002: 0-100. 0 is legitimate (it switches the display off), so only
// values above the scale are refused.
void test_led_brightness_range() {
  CHECK(led_brightness_percent_in_range(0));
  CHECK(led_brightness_percent_in_range(100));
  CHECK(led_brightness_percent_in_range(50));
  CHECK(!led_brightness_percent_in_range(101));
  CHECK(!led_brightness_percent_in_range(255));
}

// STATE-008: bit 5 (0x0020) is heater-on, bit 12 (0x1000) is pump. Bit 13
// (0x2000) is the vibration-alert pulse and must NOT read as the pump.
void test_status_register_bits() {
  CHECK(!heater_on_from_status(0x0000));
  CHECK(heater_on_from_status(0x0020));
  CHECK(heater_on_from_status(0x0023));  // heater on, bits 0/1 also set
  CHECK(!pump_on_from_status(0x0020));   // heater only

  CHECK(!pump_on_from_status(0x0000));
  CHECK(pump_on_from_status(0x1000));
  CHECK(pump_on_from_status(0x3000));   // pump running (bits 12+13)
  CHECK(!pump_on_from_status(0x2000));  // vibration pulse alone, bit 13 only
  CHECK(!heater_on_from_status(0x3000));
}

// CHAR-010 / CMD-004: inverted polarity -- the bit CLEAR means vibration is
// enabled. Register values from docs/protocol/open-questions.md's
// "Static bits in the settings registers" note.
void test_vibration_register_polarity_is_inverted() {
  CHECK(vibration_enabled_from_register(0x00000000));   // bit clear -> enabled
  CHECK(!vibration_enabled_from_register(0x00000400));  // bit set -> disabled
  CHECK(vibration_enabled_from_register(0x00010000));   // an unrelated bit set, vibration bit still clear
  CHECK(!vibration_enabled_from_register(0x00010400));
}

// CHAR-009: display-on-cooling has the same inverted polarity as vibration;
// the units bit does NOT (set = Fahrenheit). Both decode from one register
// value.
void test_display_register_polarities() {
  CHECK(display_on_cooling_enabled_from_register(0x00000000));   // bit clear -> shows current temp while cooling
  CHECK(!display_on_cooling_enabled_from_register(0x00001000));  // bit set -> shows nothing

  CHECK(!display_units_fahrenheit_from_register(0x00000000));  // clear -> Celsius
  CHECK(display_units_fahrenheit_from_register(0x00000200));   // set -> Fahrenheit

  // Fahrenheit selected AND display-on-cooling on: units bit set, cooling
  // bit clear, in one value.
  CHECK(display_units_fahrenheit_from_register(0x00000200));
  CHECK(display_on_cooling_enabled_from_register(0x00000200));
}

// STATE-007 / CMD-001: 4-byte little-endian decidegrees. A short payload is
// rejected without touching the out-param.
void test_decode_decidegrees() {
  uint32_t raw = 0xDEADBEEF;
  const uint8_t two_bytes[] = {0x00, 0x00};
  CHECK(!decode_decidegrees_c(two_bytes, sizeof(two_bytes), &raw));
  CHECK(raw == 0xDEADBEEF);  // untouched

  const uint8_t v_2300[] = {0xFC, 0x08, 0x00, 0x00};  // 0x08FC = 2300 = 230.0 C
  CHECK(decode_decidegrees_c(v_2300, sizeof(v_2300), &raw));
  CHECK(raw == 2300);

  const uint8_t v_400[] = {0x90, 0x01, 0x00, 0x00};  // 0x0190 = 400 = 40.0 C
  CHECK(decode_decidegrees_c(v_400, sizeof(v_400), &raw));
  CHECK(raw == 400);

  const uint8_t v_zero[] = {0x00, 0x00, 0x00, 0x00};  // STATE-012 "no reading"
  CHECK(decode_decidegrees_c(v_zero, sizeof(v_zero), &raw));
  CHECK(raw == 0);

  // A longer payload decodes from the first four bytes only.
  const uint8_t v_long[] = {0x2A, 0x00, 0x00, 0x00, 0xFF};  // 0x2A = 42
  CHECK(decode_decidegrees_c(v_long, sizeof(v_long), &raw));
  CHECK(raw == 42);
}

// Device-information strings are fixed-width and padded with spaces on some
// characteristics and NUL on others; both are trimmed.
void test_trim_padded_ascii() {
  const uint8_t space_padded[] = {'2', '3', '0', 'V', 'A', 'C', ' ', ' ', ' ', ' '};
  CHECK(trim_padded_ascii(space_padded, sizeof(space_padded)) == "230VAC");

  const uint8_t nul_padded[] = {'H', 'Y', 'B', 'R', 'I', 'D', 0x00, 0x00, 0x00};
  CHECK(trim_padded_ascii(nul_padded, sizeof(nul_padded)) == "HYBRID");

  const uint8_t nul_then_garbage[] = {'V', '0', '1', 0x00, 'X', 'Y'};
  CHECK(trim_padded_ascii(nul_then_garbage, sizeof(nul_then_garbage)) == "V01");

  const uint8_t all_spaces[] = {' ', ' ', ' '};
  CHECK(trim_padded_ascii(all_spaces, sizeof(all_spaces)).empty());

  CHECK(trim_padded_ascii(nullptr, 0).empty());

  // An interior space is kept -- only trailing padding is stripped.
  const uint8_t interior_space[] = {'S', '&', 'B', ' ', 'H', ' '};
  CHECK(trim_padded_ascii(interior_space, sizeof(interior_space)) == "S&B H");
}

// CMD-001: 4-byte little-endian, only the low two bytes populated.
void test_encode_target_temperature_payload() {
  uint8_t out[4] = {0xAA, 0xAA, 0xAA, 0xAA};
  encode_target_temperature_payload(2300, out);  // 230.0 C
  CHECK(out[0] == 0xFC && out[1] == 0x08 && out[2] == 0x00 && out[3] == 0x00);

  encode_target_temperature_payload(400, out);  // 40.0 C
  CHECK(out[0] == 0x90 && out[1] == 0x01 && out[2] == 0x00 && out[3] == 0x00);

  // A written value round-trips through the decoder.
  uint32_t raw = 0;
  encode_target_temperature_payload(1855, out);  // 185.5 C, sub-degree
  CHECK(decode_decidegrees_c(out, sizeof(out), &raw));
  CHECK(raw == 1855);
}

// CMD-003: 2-byte little-endian seconds.
void test_encode_auto_shutoff_duration_payload() {
  uint8_t out[2] = {0xAA, 0xAA};
  encode_auto_shutoff_duration_payload(10800, out);  // 30 2a
  CHECK(out[0] == 0x30 && out[1] == 0x2A);

  encode_auto_shutoff_duration_payload(60, out);  // 3c 00
  CHECK(out[0] == 0x3C && out[1] == 0x00);
}

// CMD-002: 2 bytes, only the low one significant.
void test_encode_led_brightness_payload() {
  uint8_t out[2] = {0xAA, 0xAA};
  encode_led_brightness_payload(100, out);  // 64 00
  CHECK(out[0] == 0x64 && out[1] == 0x00);

  encode_led_brightness_payload(0, out);
  CHECK(out[0] == 0x00 && out[1] == 0x00);
}

// CMD-004: mask-and-action form, inverted -- `00 04 00 00` turns vibration
// ON (clears the bit), `00 04 01 00` turns it OFF.
void test_encode_vibration_write_is_inverted() {
  uint8_t out[4] = {0xAA, 0xAA, 0xAA, 0xAA};
  encode_vibration_write(true, out);  // on
  CHECK(out[0] == 0x00 && out[1] == 0x04 && out[2] == 0x00 && out[3] == 0x00);

  encode_vibration_write(false, out);  // off
  CHECK(out[0] == 0x00 && out[1] == 0x04 && out[2] == 0x01 && out[3] == 0x00);
}

// CMD-005: same inverted mask-and-action form as vibration, mask 0x1000.
void test_encode_display_on_cooling_write_is_inverted() {
  uint8_t out[4] = {0xAA, 0xAA, 0xAA, 0xAA};
  encode_display_on_cooling_write(true, out);  // on
  CHECK(out[0] == 0x00 && out[1] == 0x10 && out[2] == 0x00 && out[3] == 0x00);

  encode_display_on_cooling_write(false, out);  // off
  CHECK(out[0] == 0x00 && out[1] == 0x10 && out[2] == 0x01 && out[3] == 0x00);
}

// CMD-010: NOT inverted -- `00 02 01 00` selects Fahrenheit, `00 02 00 00`
// selects Celsius. This is the case the setting-bit note warns about.
void test_encode_display_units_write_is_not_inverted() {
  uint8_t out[4] = {0xAA, 0xAA, 0xAA, 0xAA};
  encode_display_units_write(true, out);  // Fahrenheit
  CHECK(out[0] == 0x00 && out[1] == 0x02 && out[2] == 0x01 && out[3] == 0x00);

  encode_display_units_write(false, out);  // Celsius
  CHECK(out[0] == 0x00 && out[1] == 0x02 && out[2] == 0x00 && out[3] == 0x00);
}

// The write and read sides of each settings bit agree: encoding "enable"
// then reading the register back with that bit in the encoded state
// reports "enabled".
void test_settings_bit_write_read_round_trip() {
  uint8_t out[4];

  encode_vibration_write(true, out);
  // The action byte is what the register ends up carrying at that bit.
  CHECK(vibration_enabled_from_register(out[2] ? VIBRATION_BIT_DISABLED : 0));

  encode_vibration_write(false, out);
  CHECK(!vibration_enabled_from_register(out[2] ? VIBRATION_BIT_DISABLED : 0));

  encode_display_units_write(true, out);
  CHECK(display_units_fahrenheit_from_register(out[2] ? DISPLAY_UNITS_BIT_FAHRENHEIT : 0));

  encode_display_units_write(false, out);
  CHECK(!display_units_fahrenheit_from_register(out[2] ? DISPLAY_UNITS_BIT_FAHRENHEIT : 0));
}

int main() {
  test_target_temperature_range();
  test_auto_shutoff_duration_range();
  test_led_brightness_range();
  test_status_register_bits();
  test_vibration_register_polarity_is_inverted();
  test_display_register_polarities();
  test_decode_decidegrees();
  test_trim_padded_ascii();
  test_encode_target_temperature_payload();
  test_encode_auto_shutoff_duration_payload();
  test_encode_led_brightness_payload();
  test_encode_vibration_write_is_inverted();
  test_encode_display_on_cooling_write_is_inverted();
  test_encode_display_units_write_is_not_inverted();
  test_settings_bit_write_read_round_trip();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
