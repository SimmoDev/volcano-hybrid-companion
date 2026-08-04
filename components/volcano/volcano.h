#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

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

  // Reads CHAR-017, whose value nothing else would otherwise reveal: it has
  // no notify, so without this the configured duration is unknown until
  // something writes one. Issued once per connection, and again after each
  // write as the read-back that confirms it.
  void read_auto_shutoff_duration_();

  // Optional publish targets set via the set_*() methods above. Null unless
  // configured in YAML.
  sensor::Sensor *current_temperature_sensor_{nullptr};
  sensor::Sensor *auto_shutoff_countdown_sensor_{nullptr};
  VolcanoTargetTemperatureNumber *target_temperature_number_{nullptr};
  VolcanoAutoShutoffDurationNumber *auto_shutoff_duration_number_{nullptr};
  VolcanoHeaterSwitch *heater_switch_{nullptr};
  VolcanoPumpSwitch *pump_switch_{nullptr};

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
