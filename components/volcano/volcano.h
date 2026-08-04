#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"

#include <string>

#include <esp_gattc_api.h>

namespace esphome {
namespace volcano {

class VolcanoComponent;

// Writable target temperature (CHAR-014; CMD-001 for the write, STATE-013
// for the notify side). One entity for both directions: setting it writes
// the target, and the component publishes back to it whenever the device
// reports a new one -- including targets set at the device's own panel,
// which STATE-013 confirms are notified.
//
// The configured min/max are what the entity advertises, and ESPHome clamps
// to them before control() is reached. They are not the safety bound: that
// is enforced in set_target_temperature_decidegrees() against the range
// CMD-001 confirms accepted, whatever this entity is configured to span.
class VolcanoTargetTemperatureNumber : public number::Number, public Parented<VolcanoComponent> {
 protected:
  void control(float value) override;
};

// Writable auto-shutoff duration (CHAR-017, CMD-003), in minutes -- the unit
// the official app presents it in, converted here to the seconds CMD-003
// encodes. Distinct from the auto-shutoff *countdown* (CHAR-016, STATE-005),
// which is the live time remaining and is read-only: this is the value the
// countdown loads at its next arming.
//
// CHAR-017 has no notify, so this is populated by an explicit read on each
// connection and refreshed by the read-back after each write. The same
// min/max caveat as VolcanoTargetTemperatureNumber applies.
class VolcanoAutoShutoffDurationNumber : public number::Number, public Parented<VolcanoComponent> {
 protected:
  void control(float value) override;
};

// Writable LED brightness (CHAR-015, CMD-002), on the 0-100 scale the
// characteristic reads back and the write uses. Read/Write with no notify,
// so this is populated by a read on each connection and refreshed by the
// read-back after each write, like the auto-shutoff duration.
class VolcanoLedBrightnessNumber : public number::Number, public Parented<VolcanoComponent> {
 protected:
  void control(float value) override;
};

// Heater and pump as one entity each, rather than a state readout plus a
// pair of on/off triggers. Turning either on or off writes the matching
// trigger characteristic (CMD-006 through CMD-009), and the component
// publishes back to them from the status/flags register (STATE-008) -- so
// they follow the device whatever changed it, including its own panel.
//
// Neither ever publishes optimistically: the state shown is the one the
// device reported, not the one that was asked for. STATE-005 and STATE-011
// make that distinction matter -- the device switches its own actuators off
// at auto-shutoff expiry, with no command involved.
//
// Their restore mode must stay DISABLED (see __init__.py). A switch that
// restored its previous state would actuate the heater or pump at boot,
// before the status register has said what the device is actually doing.
class VolcanoHeaterSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

class VolcanoPumpSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

// Vibration setting (CHAR-010, CMD-004). A setting rather than an actuator:
// it governs whether the device buzzes on reaching temperature, so it
// defaults to the config entity category.
//
// Note the inverted polarity this hides. The register bit is *clear* when
// vibration is enabled and set when it is disabled, so "on" here writes the
// clear action and decodes from an absent bit. See set_vibration() and
// decode_vibration_() in volcano.cpp.
class VolcanoVibrationSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

// Display-on-cooling setting (CHAR-009, CMD-005): whether the device shows
// the current temperature on its own display while cooling. Display-only --
// current temperature keeps notifying over BLE either way -- so it affects
// nothing this component reads.
//
// Its bit is inverted like the vibration setting's, and it shares a register
// with the display units bit (STATE-010) and the temperature-step pulse
// (STATE-009), neither of which is decoded here.
class VolcanoDisplayOnCoolingSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

// VolcanoComponent is the root of the Volcano component defined in
// ADR-0002 (docs/decisions/ADR-0002-volcano-component-architecture.md).
//
// This is the BLE communication layer's foundation, following the
// connect / resolve / subscribe / read / decode path proved by the first
// increment on a growing set of characteristics: the
// status/flags register (CHAR-008, decoded in
// docs/protocol/state-model.md#state-008-statusflags-register-partial),
// the auto-shutoff countdown (CHAR-016, decoded in
// docs/protocol/state-model.md#state-005--auto-shutoff-countdown), and
// current/target temperature (CHAR-013/CHAR-014, decoded in
// docs/protocol/state-model.md#state-007--current-actual-temperature).
// Current temperature reads `0` whenever the heater is off below 40 degC
// (STATE-012); that is logged as "no reading", never as a temperature.
//
// It also reads the heater-runtime meter -- hours and minutes of
// operation (CHAR-022/CHAR-023, STATE-001/STATE-006), which advance
// only while the heater is on and are subscribed like the values above.
//
// It also reads the device-information strings once per connection --
// firmware version (CHAR-005, STATE-003), firmware BLE version (CHAR-006,
// STATE-004), serial number (CHAR-007, STATE-002), power supply rating
// (CHAR-024) and product line name (CHAR-025). None of them notify, and
// none is expected to change while connected. Three of the five are
// writable on the device and none is ever written here; CHAR-007 in
// particular would overwrite a real unit's serial number.
//
// It also carries the first production writes. set_auto_shutoff_duration_seconds()
// writes the auto-shutoff duration (CHAR-017, CMD-003 in
// docs/protocol/commands.md). CMD-003 is Confirmed from 60 seconds --
// accepted, read back unchanged, loaded at the next arming, and honoured
// through to an actual expiry -- up to the 21600 seconds topping the
// official app's own range, so this refuses anything outside that rather
// than writing an untested value: 0 in particular is unverified and may
// mean "disabled" on this device, which would silently remove the only
// backstop ADR-0007's persistent-connection design relies on.
//
// turn_heater_on()/turn_heater_off()/turn_pump_on()/turn_pump_off() write the
// four one-byte trigger characteristics (CHAR-018 through CHAR-021,
// CMD-006 through CMD-009), each Confirmed to accept the single value `0x00`
// -- the only value ever observed written to them -- so that is the only
// value these write. This is the first code in this component that can
// actuate the heater, which is why the auto-shutoff floor above matters here
// specifically.
//
// set_target_temperature_decidegrees() writes the target temperature
// (CHAR-014, CMD-001). Only the 40.0-230.0 degC range the official app's UI
// exposes is Confirmed as accepted; what the device does with a value
// outside it is Unknown, so this refuses anything outside that range rather
// than writing an untested value (ADR-0005).
//
// Connection lifecycle follows ADR-0007
// (docs/decisions/ADR-0007-ble-connection-lifecycle.md): the parent
// ble_client holds a persistent connection and reconnects on its own, so
// this component re-resolves every characteristic and re-reads its value
// on every fresh connection (CONN-002, ADR-0007) rather than assuming
// state carries over, and treats heater/pump state as unknown while
// disconnected.
//
// Every decoded value above is also published to an optional ESPHome entity,
// configured directly under the `volcano:` block (see __init__.py), if one
// is configured for it -- e.g. so it can show on an ESPHome web_server page,
// per components/volcano/README.md. Read-only values get a sensor; the
// writable ones get an entity that both reports the device's current value
// and writes a new one -- a number for target temperature and auto-shutoff
// duration, a switch for the heater and pump. This is a stopgap: it exposes
// decoded state and the existing writes, not a hardware-independent Volcano
// domain interface, which is still the TODO below.
//
// TODO(volcano-component): the remaining characteristics, the full Volcano
// device state model, and the hardware-independent interface for control
// interfaces (all per ADR-0002) are not yet implemented -- this component
// currently only logs and optionally publishes decoded state, and writes
// the characteristics documented above.
class VolcanoComponent : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                            esp_ble_gattc_cb_param_t *param) override;

  // Writes CMD-003's confirmed 2-byte-seconds encoding to the auto-shutoff
  // duration characteristic (see the class comment above for the confirmed
  // range this refuses to go outside of). A no-op, logged, if `seconds` is
  // outside that range or no connection is established.
  void set_auto_shutoff_duration_seconds(uint16_t seconds);

  // CMD-006 through CMD-009: each writes `0x00` to its trigger
  // characteristic. A no-op, logged, if no connection is established.
  void turn_heater_on();
  void turn_heater_off();
  void turn_pump_on();
  void turn_pump_off();

  // Writes CMD-001's confirmed 4-byte-deci-degrees-Celsius encoding to the
  // target temperature characteristic (see the class comment above for the
  // confirmed range this refuses to go outside of).
  void set_target_temperature_decidegrees(uint16_t decidegrees);

  // Writes CMD-002's confirmed 2-byte encoding to the LED brightness
  // characteristic. A no-op, logged, if `percent` is outside the 0-100
  // scale the finding records, or no connection is established.
  void set_led_brightness_percent(uint8_t percent);

  // Writes CMD-004's confirmed mask-and-action form to the vibration
  // register. `enabled` is the user-facing sense: the bit written is its
  // inverse, per CHAR-010's inverted polarity.
  void set_vibration(bool enabled);

  // Writes CMD-005's confirmed mask-and-action form to the display/units
  // register, naming only the display-on-cooling bit. `enabled` is the
  // user-facing sense; the bit written is its inverse.
  void set_display_on_cooling(bool enabled);

  // Optional sinks for decoded state, set by __init__.py from the `volcano:`
  // block's optional entity sub-schemas. Each is published to from the
  // corresponding decode_*_() method below when configured. The two number
  // entities are read/write: they publish device state the same way, and
  // their control() writes back through the setters above.
  void set_current_temperature_sensor(sensor::Sensor *s) { current_temperature_sensor_ = s; }
  void set_auto_shutoff_countdown_sensor(sensor::Sensor *s) { auto_shutoff_countdown_sensor_ = s; }
  void set_target_temperature_number(VolcanoTargetTemperatureNumber *n) { target_temperature_number_ = n; }
  void set_auto_shutoff_duration_number(VolcanoAutoShutoffDurationNumber *n) { auto_shutoff_duration_number_ = n; }
  void set_heater_switch(VolcanoHeaterSwitch *s) { heater_switch_ = s; }
  void set_pump_switch(VolcanoPumpSwitch *s) { pump_switch_ = s; }
  void set_vibration_switch(VolcanoVibrationSwitch *s) { vibration_switch_ = s; }
  void set_display_on_cooling_switch(VolcanoDisplayOnCoolingSwitch *s) { display_on_cooling_switch_ = s; }
  void set_firmware_version_text_sensor(text_sensor::TextSensor *s) { firmware_version_text_sensor_ = s; }
  void set_ble_firmware_version_text_sensor(text_sensor::TextSensor *s) { ble_firmware_version_text_sensor_ = s; }
  void set_serial_number_text_sensor(text_sensor::TextSensor *s) { serial_number_text_sensor_ = s; }
  void set_power_supply_text_sensor(text_sensor::TextSensor *s) { power_supply_text_sensor_ = s; }
  void set_product_line_text_sensor(text_sensor::TextSensor *s) { product_line_text_sensor_ = s; }
  void set_led_brightness_number(VolcanoLedBrightnessNumber *n) { led_brightness_number_ = n; }
  void set_hours_sensor(sensor::Sensor *s) { hours_sensor_ = s; }
  void set_minutes_sensor(sensor::Sensor *s) { minutes_sensor_ = s; }

 protected:
  void decode_status_(const uint8_t *value, uint16_t value_len);
  void decode_countdown_(const uint8_t *value, uint16_t value_len);
  void decode_current_temperature_(const uint8_t *value, uint16_t value_len);
  void decode_target_temperature_(const uint8_t *value, uint16_t value_len);

  // Handles resolved by UUID after each connection. Zero while
  // unresolved/disconnected.
  uint16_t status_handle_{0};         // CHAR-008: status/flags register.
  uint16_t countdown_handle_{0};      // CHAR-016: auto-shutoff countdown.
  uint16_t duration_handle_{0};       // CHAR-017: auto-shutoff duration.
  uint16_t current_temp_handle_{0};   // CHAR-013: current (actual) temperature.
  uint16_t target_temp_handle_{0};    // CHAR-014: target temperature.
  uint16_t heater_on_handle_{0};      // CHAR-018: heater on trigger.
  uint16_t heater_off_handle_{0};     // CHAR-019: heater off trigger.
  uint16_t pump_on_handle_{0};        // CHAR-020: pump on trigger.
  uint16_t pump_off_handle_{0};       // CHAR-021: pump off trigger.
  // Device information, all read-only in practice. CHAR-007, CHAR-024 and
  // CHAR-025 are writable on the device, and must never be written: CHAR-007
  // in particular would overwrite a real unit's serial number.
  uint16_t firmware_version_handle_{0};      // CHAR-005: firmware version.
  uint16_t ble_firmware_version_handle_{0};  // CHAR-006: firmware BLE version.
  uint16_t serial_number_handle_{0};         // CHAR-007: serial number.
  uint16_t power_supply_handle_{0};          // CHAR-024: power supply rating.
  uint16_t product_line_handle_{0};          // CHAR-025: product line name.
  // The heater-runtime meter, notify-capable and so subscribed rather
  // than read once. Both advance only while the heater is on.
  uint16_t hours_handle_{0};                 // CHAR-022: hours of operation.
  uint16_t minutes_handle_{0};               // CHAR-023: minutes of operation.
  uint16_t led_brightness_handle_{0};        // CHAR-015: LED brightness.
  uint16_t vibration_handle_{0};             // CHAR-010: vibration setting.
  uint16_t display_register_handle_{0};      // CHAR-009: display/units register.

  // Reads CHAR-017, whose value nothing else would otherwise reveal: it has
  // no notify, so without this the configured duration is unknown until
  // something writes one. Issued once per connection as part of the static
  // read sequence below, and again after each write as the read-back that
  // confirms it.
  void read_auto_shutoff_duration_();

  void decode_vibration_(const uint8_t *value, uint16_t value_len);
  void decode_display_register_(const uint8_t *value, uint16_t value_len);
  void decode_hours_(const uint8_t *value, uint16_t value_len);
  void decode_minutes_(const uint8_t *value, uint16_t value_len);
  void decode_text_(const uint8_t *value, uint16_t value_len, text_sensor::TextSensor *sensor, const char *name);

  // None of the device-information characteristics notify, so each is read
  // once per connection. They are read one at a time, each issued from the
  // previous one's completion, rather than fired off together: a GATT client
  // has only a small number of outstanding reads available, and this queue
  // shares that budget with the reads the subscriptions already issue.
  void queue_static_reads_();
  void issue_next_static_read_();

  static const uint8_t MAX_STATIC_READS = 7;
  uint16_t static_reads_[MAX_STATIC_READS];
  uint8_t static_read_count_{0};
  uint8_t static_read_index_{0};

  // Optional publish targets set via the set_*() methods above. Null unless
  // configured in YAML.
  sensor::Sensor *current_temperature_sensor_{nullptr};
  sensor::Sensor *auto_shutoff_countdown_sensor_{nullptr};
  VolcanoTargetTemperatureNumber *target_temperature_number_{nullptr};
  VolcanoAutoShutoffDurationNumber *auto_shutoff_duration_number_{nullptr};
  VolcanoHeaterSwitch *heater_switch_{nullptr};
  VolcanoPumpSwitch *pump_switch_{nullptr};
  VolcanoVibrationSwitch *vibration_switch_{nullptr};
  VolcanoDisplayOnCoolingSwitch *display_on_cooling_switch_{nullptr};

  // Last decoded display-on-cooling state, or -1 before the first read.
  // Its register notifies on every 1 degC change of current temperature
  // (STATE-009), not only when the setting changes, so this exists to
  // keep the log to actual changes rather than a line every few seconds
  // throughout a heating or cooling run.
  int8_t display_on_cooling_state_{-1};
  text_sensor::TextSensor *firmware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *ble_firmware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_text_sensor_{nullptr};
  text_sensor::TextSensor *power_supply_text_sensor_{nullptr};
  text_sensor::TextSensor *product_line_text_sensor_{nullptr};
  VolcanoLedBrightnessNumber *led_brightness_number_{nullptr};
  sensor::Sensor *hours_sensor_{nullptr};
  sensor::Sensor *minutes_sensor_{nullptr};

  // Number of ESP_GATTC_REG_FOR_NOTIFY_EVT callbacks still outstanding.
  // node_state must not become ESTABLISHED until this reaches zero: the
  // parent ble_client releases its GATT service/characteristic cache once
  // every node reports ESTABLISHED, and the CCCD-descriptor lookup each
  // pending subscription still needs (performed internally by
  // esp32_ble_client::BLEClientBase for every REG_FOR_NOTIFY_EVT) reads
  // that same cache -- reporting ESTABLISHED early releases it out from
  // under a subscription still in flight and crashes.
  uint8_t pending_subscriptions_{0};
};

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
