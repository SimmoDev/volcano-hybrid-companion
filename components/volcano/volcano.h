#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"

#include "volcano_ble_client.h"
#include "volcano_device.h"

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
// is enforced in VolcanoBleClient::write_target_temperature() against the
// range CMD-001 confirms accepted, whatever this entity is configured to
// span.
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
// Per ADR-0009, neither switch can express "unknown" -- ESPHome's switch
// entity has no such state -- so each holds its last published value across
// a disconnect rather than being forced to a value. The `connected` binary
// sensor (see VolcanoComponent below) is what a consumer should trust
// instead. Their restore mode must stay DISABLED (see __init__.py): a
// switch that restored its previous state at boot would actuate the heater
// or pump before the device has said what it is actually doing.
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
// defaults to the config entity category. Holds its last value across a
// disconnect, like the heater/pump switches above.
class VolcanoVibrationSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

// Display-on-cooling setting (CHAR-009, CMD-005): whether the device shows
// the current temperature on its own display while cooling. Display-only --
// current temperature keeps notifying over BLE either way -- so it affects
// nothing this component reads. Holds its last value across a disconnect,
// like the switches above.
class VolcanoDisplayOnCoolingSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

// Display units (STATE-010, bit 9 of CHAR-009's register; CMD-010 for the
// write). ON means Fahrenheit. The device's own simultaneous +/- panel
// gesture changes the same setting, and this switch follows it. Holds its
// last value across a disconnect, like the switches above.
class VolcanoDisplayUnitsSwitch : public switch_::Switch, public Parented<VolcanoComponent> {
 protected:
  void write_state(bool state) override;
};

// VolcanoComponent is the ESPHome integration named in ADR-0002
// (docs/decisions/ADR-0002-volcano-component-architecture.md) and ADR-0009
// (docs/decisions/ADR-0009-volcano-abstraction-layer-interface.md). It owns
// Component/ble_client::BLEClientNode participation, a VolcanoBleClient
// (the BLE communication layer) and a VolcanoDevice (the Volcano
// abstraction layer, the single authoritative state model), and the
// optional ESPHome entities configured under the `volcano:` block --
// itself just one consumer of VolcanoDevice's interface, on equal footing
// with any other control interface built against it directly.
//
// This class does no protocol work of its own: gattc_event_handler()
// forwards into VolcanoBleClient, and every set_*() below forwards into
// VolcanoDevice. Its job is entity wiring -- translating YAML-configured
// entities' control()/write_state() calls into VolcanoDevice calls, and
// VolcanoDevice's state-change callback into publish_state() calls on
// whichever entities are configured -- documented in
// components/volcano/README.md.
class VolcanoComponent : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  // Forwarded to the matching VolcanoDevice::set_*() -- see volcano_device.h
  // for the confirmed ranges each refuses outside of, and ADR-0009 for the
  // requested-versus-confirmed handling every write goes through.
  void set_target_temperature(float celsius);
  void set_auto_shutoff_duration_seconds(uint16_t seconds);
  void set_led_brightness_percent(uint8_t percent);
  void set_vibration(bool enabled);
  void set_display_on_cooling(bool enabled);
  void set_display_units_fahrenheit(bool fahrenheit);
  void set_heater(bool on);
  void set_pump(bool on);

  // Optional sinks for VolcanoDevice's state, set by __init__.py from the
  // `volcano:` block's optional entity sub-schemas. Each is published to
  // from on_device_state_changed_() below when configured.
  void set_current_temperature_sensor(sensor::Sensor *s) { current_temperature_sensor_ = s; }
  void set_auto_shutoff_countdown_sensor(sensor::Sensor *s) { auto_shutoff_countdown_sensor_ = s; }
  void set_target_temperature_number(VolcanoTargetTemperatureNumber *n) { target_temperature_number_ = n; }
  void set_auto_shutoff_duration_number(VolcanoAutoShutoffDurationNumber *n) { auto_shutoff_duration_number_ = n; }
  void set_heater_switch(VolcanoHeaterSwitch *s) { heater_switch_ = s; }
  void set_pump_switch(VolcanoPumpSwitch *s) { pump_switch_ = s; }
  void set_vibration_switch(VolcanoVibrationSwitch *s) { vibration_switch_ = s; }
  void set_display_on_cooling_switch(VolcanoDisplayOnCoolingSwitch *s) { display_on_cooling_switch_ = s; }
  void set_display_units_switch(VolcanoDisplayUnitsSwitch *s) { display_units_switch_ = s; }
  void set_firmware_version_text_sensor(text_sensor::TextSensor *s) { firmware_version_text_sensor_ = s; }
  void set_ble_firmware_version_text_sensor(text_sensor::TextSensor *s) { ble_firmware_version_text_sensor_ = s; }
  void set_serial_number_text_sensor(text_sensor::TextSensor *s) { serial_number_text_sensor_ = s; }
  void set_power_supply_text_sensor(text_sensor::TextSensor *s) { power_supply_text_sensor_ = s; }
  void set_product_line_text_sensor(text_sensor::TextSensor *s) { product_line_text_sensor_ = s; }
  void set_led_brightness_number(VolcanoLedBrightnessNumber *n) { led_brightness_number_ = n; }
  void set_hours_sensor(sensor::Sensor *s) { hours_sensor_ = s; }
  void set_minutes_sensor(sensor::Sensor *s) { minutes_sensor_ = s; }
  void set_connected_binary_sensor(binary_sensor::BinarySensor *s) { connected_binary_sensor_ = s; }

 protected:
  // Fires on any VolcanoDevice state change. Publishes only where a
  // configured entity's own cached value actually differs from what
  // VolcanoDevice now reports -- see volcano.cpp for the per-entity-type
  // handling ADR-0009 requires (sensor/number publish NAN when unknown,
  // text_sensor clears has_state(), a switch is left untouched when
  // unknown since ESPHome's switch entity cannot express that itself).
  void on_device_state_changed_();

  VolcanoBleClient ble_client_;
  VolcanoDevice device_;

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
  VolcanoDisplayUnitsSwitch *display_units_switch_{nullptr};
  text_sensor::TextSensor *firmware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *ble_firmware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_text_sensor_{nullptr};
  text_sensor::TextSensor *power_supply_text_sensor_{nullptr};
  text_sensor::TextSensor *product_line_text_sensor_{nullptr};
  VolcanoLedBrightnessNumber *led_brightness_number_{nullptr};
  sensor::Sensor *hours_sensor_{nullptr};
  sensor::Sensor *minutes_sensor_{nullptr};
  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
};

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
