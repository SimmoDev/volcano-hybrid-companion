#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace volcano {

// Fields VolcanoDevice can request a write for. Shared between the BLE
// communication layer and the abstraction layer (ADR-0009) so a write
// failure can be reported against a field without either layer depending
// on the other's concrete type.
enum class VolcanoField {
  TARGET_TEMPERATURE,
  AUTO_SHUTOFF_DURATION,
  LED_BRIGHTNESS,
  HEATER,
  PUMP,
  VIBRATION,
  DISPLAY_ON_COOLING,
  DISPLAY_UNITS_FAHRENHEIT,
  // Sentinel: the number of real fields above, never a field itself. Lets
  // VolcanoDevice::pending_deadlines_ size itself from this enum instead of
  // a hand-counted literal that could silently fall out of sync with it.
  COUNT,
};

// Declared by VolcanoBleClient, implemented by VolcanoDevice (ADR-0009).
// Every method reports a decoded domain value, never a raw payload --
// decidegrees, register masks and characteristic handles stay inside
// VolcanoBleClient. This header has no BLE/ESP-IDF dependency, so
// VolcanoDevice can implement it without pulling any of that in.
class VolcanoBleClientObserver {
 public:
  virtual ~VolcanoBleClientObserver() = default;

  // Connection lifecycle. on_connecting() fires once service discovery
  // completes and characteristic resolution/subscription begins; on_ready()
  // fires once every subscription has settled and the static read sweep
  // (device information, LED brightness, auto-shutoff duration) has
  // completed; on_disconnected() fires on link loss.
  virtual void on_connecting() = 0;
  virtual void on_ready() = 0;
  virtual void on_disconnected() = 0;

  // STATE-008: heater and pump decode from one register read/notification.
  virtual void on_actuator_state(bool heater_on, bool pump_on) = 0;

  // STATE-007/STATE-012: has_reading is false for the sub-40 degC "no
  // reading" gate, in which case celsius carries no meaning.
  virtual void on_current_temperature(bool has_reading, float celsius) = 0;

  // CMD-001/STATE-013: reported from a read-back after a write or a
  // device-originated (e.g. panel) notification.
  virtual void on_target_temperature(float celsius) = 0;

  // STATE-005: live countdown, seconds.
  virtual void on_auto_shutoff_countdown(uint16_t seconds) = 0;

  // CHAR-017: configured duration, seconds -- read once per connection and
  // again as the read-back confirming a write.
  virtual void on_auto_shutoff_duration(uint16_t seconds) = 0;

  // CHAR-015: 0-100, read once per connection and again as the read-back
  // confirming a write.
  virtual void on_led_brightness(uint8_t percent) = 0;

  // CHAR-010/CMD-004: already un-inverted from the register's polarity.
  virtual void on_vibration(bool enabled) = 0;

  // CHAR-009: display-on-cooling and the Celsius/Fahrenheit units bit
  // decode from the same register value, so both are reported together.
  virtual void on_display_register(bool display_on_cooling, bool display_units_fahrenheit) = 0;

  // STATE-001/STATE-006: the heater-runtime meter.
  virtual void on_hours_of_operation(uint32_t hours) = 0;
  virtual void on_minutes_of_operation(uint16_t minutes) = 0;

  // Device information (STATE-002/003/004, CHAR-024/025), each reported
  // once per connection. VolcanoDevice, not VolcanoBleClient, is what
  // exempts these from going unknown across a disconnect.
  virtual void on_firmware_version(const std::string &value) = 0;
  virtual void on_ble_firmware_version(const std::string &value) = 0;
  virtual void on_serial_number(const std::string &value) = 0;
  virtual void on_power_supply(const std::string &value) = 0;
  virtual void on_product_line(const std::string &value) = 0;

  // An immediate GATT-level write failure (status != ESP_GATT_OK) for the
  // named field. Distinct from a timeout: this is a same-tick negative
  // signal, matching what a failed write already logs and gives up on --
  // no read-back is attempted after one. A confirmed-different-value
  // outcome (STATE-013's silent drop) is not reported here; VolcanoDevice
  // infers it by comparing a later on_*() report against what it requested.
  virtual void on_write_failed(VolcanoField field) = 0;
};

}  // namespace volcano
}  // namespace esphome
