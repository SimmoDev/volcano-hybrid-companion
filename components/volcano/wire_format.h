#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace volcano {

// Pure wire-format helpers for the Volcano Hybrid's BLE characteristics,
// split out of volcano_ble_client.cpp so the encoding, decoding, bit
// polarities and range checks -- the part where a wrong mask, an inverted
// bit or a loose bound actuates real hardware or silently reverses a
// setting -- are host-testable without ESPHome or a BLE stack, the same
// split display_register_write_queue.h and static_read_queue.h already
// make. The esp_ble_gattc_* calls, characteristic resolution and observer
// dispatch stay in VolcanoBleClient; only value-level format logic lives
// here. See components/volcano/test/wire_format_test.cpp.

// --- Confirmed-accepted write ranges --------------------------------------

// CMD-001: the official app's UI spans 40.0-230.0 degC (400-2300 in this
// characteristic's deci-degrees-Celsius encoding); that is the only range
// Confirmed accepted. What the device does with a value outside it is
// Unknown, so a write outside it is refused rather than sent (ADR-0005).
inline constexpr uint16_t MIN_TARGET_TEMPERATURE_DECIDEGREES = 400;
inline constexpr uint16_t MAX_TARGET_TEMPERATURE_DECIDEGREES = 2300;

// CMD-003: the auto-shutoff duration range confirmed accepted. The floor is
// the lowest value verified read back unchanged, loaded at the next arming,
// and honoured through to an actual expiry; below it is unverified -- 0 in
// particular may mean "disabled" on this device. The ceiling is the top of
// the official app's own UI range, with writes at that end captured; above
// it is untested. Both ends are refused rather than written (ADR-0005).
inline constexpr uint16_t MIN_AUTO_SHUTOFF_DURATION_SECONDS = 60;
inline constexpr uint16_t MAX_AUTO_SHUTOFF_DURATION_SECONDS = 21600;

// CMD-002: the scale the LED brightness characteristic is Confirmed to use,
// both written and read back. 0 is a legitimate value rather than one to
// guard against -- it switches the display off entirely rather than dimming
// it, and any non-zero write restores it at the level written -- so it is
// allowed, and only values above the scale are refused.
inline constexpr uint8_t MAX_LED_BRIGHTNESS_PERCENT = 100;

inline bool target_temperature_decidegrees_in_range(long decidegrees) {
  return decidegrees >= MIN_TARGET_TEMPERATURE_DECIDEGREES && decidegrees <= MAX_TARGET_TEMPERATURE_DECIDEGREES;
}

inline bool auto_shutoff_duration_seconds_in_range(uint16_t seconds) {
  return seconds >= MIN_AUTO_SHUTOFF_DURATION_SECONDS && seconds <= MAX_AUTO_SHUTOFF_DURATION_SECONDS;
}

inline bool led_brightness_percent_in_range(uint8_t percent) { return percent <= MAX_LED_BRIGHTNESS_PERCENT; }

// --- Status/flags register (CHAR-008, STATE-008), read-only ---------------

// Bit 5 is set whenever the heater is on, clear when off.
inline constexpr uint16_t STATUS_BIT_HEATER_ON = 0x0020;
// Bit 12 marks the pump specifically. Bit 13 is also pulsed by the vibration
// alert for about a second, so it must not be used to detect the pump.
inline constexpr uint16_t STATUS_BIT_PUMP_ON = 0x1000;

inline bool heater_on_from_status(uint16_t status) { return (status & STATUS_BIT_HEATER_ON) != 0; }
inline bool pump_on_from_status(uint16_t status) { return (status & STATUS_BIT_PUMP_ON) != 0; }

// --- Settings registers (CHAR-010 vibration, CHAR-009 display/units) ------

// CHAR-010/CMD-004: the vibration setting's bit within its register.
//
// The polarity is inverted, and getting it backwards would silently invert
// the whole feature: the bit is CLEAR when vibration is ENABLED and set when
// it is disabled. CMD-004's captured writes say the same thing from the
// write side -- `00 04 00 00` (clear the bit) turns vibration on and
// `00 04 01 00` (set it) turns it off.
//
// The inversion is a property of this setting rather than of the register:
// the units bit on the sibling register is not inverted (STATE-010), so a
// new bit's polarity has to be established rather than assumed from this one.
inline constexpr uint16_t VIBRATION_BIT_DISABLED = 0x0400;

// CHAR-009/CMD-005: the display-on-cooling bit, with the same inverted
// polarity as the vibration bit -- clear means the device shows current
// temperature while cooling, set means it shows none. The register also
// carries the units bit below, a bit that pulses on every 1 degC change of
// current temperature (STATE-009), and bits still unidentified; the
// mask-and-action write form names only one bit, so writing one setting
// cannot disturb the others.
inline constexpr uint16_t DISPLAY_ON_COOLING_BIT_DISABLED = 0x1000;

// STATE-010: the display units bit on the same register. Its polarity is the
// opposite way round to the two settings above -- set means Fahrenheit,
// clear means Celsius, with no inversion -- which is exactly the trap the
// setting-bit note in docs/protocol/commands.md warns about: the inversion
// is a property of those two settings, not of the register.
inline constexpr uint16_t DISPLAY_UNITS_BIT_FAHRENHEIT = 0x0200;

inline bool vibration_enabled_from_register(uint32_t reg) { return (reg & VIBRATION_BIT_DISABLED) == 0; }

inline bool display_on_cooling_enabled_from_register(uint32_t reg) {
  return (reg & DISPLAY_ON_COOLING_BIT_DISABLED) == 0;
}

inline bool display_units_fahrenheit_from_register(uint32_t reg) { return (reg & DISPLAY_UNITS_BIT_FAHRENHEIT) != 0; }

// --- Scalar decode ------------------------------------------------------

// STATE-007/CMD-001: both temperature characteristics share a 4-byte
// little-endian encoding in units of 0.1 degC. Returns false, leaving
// *out_raw untouched, if the payload is too short.
inline bool decode_decidegrees_c(const uint8_t *value, uint16_t value_len, uint32_t *out_raw) {
  if (value_len < 4)
    return false;
  *out_raw = (static_cast<uint32_t>(value[3]) << 24) | (static_cast<uint32_t>(value[2]) << 16) |
             (static_cast<uint32_t>(value[1]) << 8) | static_cast<uint32_t>(value[0]);
  return true;
}

// Device-information strings (STATE-002/003/004, CHAR-024/025) are
// fixed-width ASCII padded to their full width -- with spaces on some
// characteristics and zero characters on others -- so both forms of padding
// are trimmed rather than reported as part of the value.
inline std::string trim_padded_ascii(const uint8_t *value, uint16_t value_len) {
  std::string text;
  text.reserve(value_len);
  for (uint16_t i = 0; i < value_len; i++) {
    if (value[i] == '\0')
      break;
    text.push_back(static_cast<char>(value[i]));
  }
  while (!text.empty() && text.back() == ' ')
    text.pop_back();
  return text;
}

// --- Write payload encode ----------------------------------------------

// CMD-001's confirmed encoding: 4-byte little-endian deci-degrees Celsius,
// matching decode_decidegrees_c(). Only the low two bytes are ever
// populated.
inline void encode_target_temperature_payload(uint16_t decidegrees, uint8_t out[4]) {
  out[0] = static_cast<uint8_t>(decidegrees & 0xFF);
  out[1] = static_cast<uint8_t>((decidegrees >> 8) & 0xFF);
  out[2] = 0;
  out[3] = 0;
}

// CMD-003's confirmed encoding: 2-byte little-endian seconds.
inline void encode_auto_shutoff_duration_payload(uint16_t seconds, uint8_t out[2]) {
  out[0] = static_cast<uint8_t>(seconds & 0xFF);
  out[1] = static_cast<uint8_t>((seconds >> 8) & 0xFF);
}

// CMD-002's confirmed encoding: 2 bytes, only the low one significant.
inline void encode_led_brightness_payload(uint8_t percent, uint8_t out[2]) {
  out[0] = percent;
  out[1] = 0;
}

// CMD-004/005/010's confirmed mask-and-action form: a 2-byte little-endian
// bit mask, then 00 to clear that bit or 01 to set it, then a padding byte.
// The write names only the bit being changed, so the register's other bits
// -- several unidentified -- are neither read first nor disturbed. The three
// wrappers below bake in each setting's mask and polarity so neither can be
// got wrong at a call site.
inline void encode_setting_write(uint16_t bit_mask, bool set_bit, uint8_t out[4]) {
  out[0] = static_cast<uint8_t>(bit_mask & 0xFF);
  out[1] = static_cast<uint8_t>((bit_mask >> 8) & 0xFF);
  out[2] = set_bit ? 0x01 : 0x00;
  out[3] = 0x00;
}

// Clear enables, per VIBRATION_BIT_DISABLED.
inline void encode_vibration_write(bool enabled, uint8_t out[4]) {
  encode_setting_write(VIBRATION_BIT_DISABLED, !enabled, out);
}

// Clear enables, as for vibration.
inline void encode_display_on_cooling_write(bool enabled, uint8_t out[4]) {
  encode_setting_write(DISPLAY_ON_COOLING_BIT_DISABLED, !enabled, out);
}

// Not inverted: set selects Fahrenheit.
inline void encode_display_units_write(bool fahrenheit, uint8_t out[4]) {
  encode_setting_write(DISPLAY_UNITS_BIT_FAHRENHEIT, fahrenheit, out);
}

}  // namespace volcano
}  // namespace esphome
