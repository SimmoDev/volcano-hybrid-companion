#pragma once

#include "volcano_ble_client_observer.h"

#include "esphome/core/optional.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace volcano {

// A single piece of device state, per ADR-0009: known only once a live
// value has actually been reported, and never surfaced as though it were
// still current once it goes unknown. requested() carries a value a
// set_*() call on VolcanoDevice has asked for but that has not yet been
// confirmed (or rejected, or timed out) by the device.
template<typename T> class DeviceValue {
 public:
  bool is_known() const { return this->known_; }
  T value() const { return this->value_; }
  optional<T> requested() const { return this->requested_; }

 private:
  friend class VolcanoDevice;
  bool known_{false};
  T value_{};
  optional<T> requested_{};
};

// ADR-0009. READY means the state model is populated -- the link is up,
// every notify-capable characteristic has subscribed, and the static read
// sweep (device information, LED brightness, auto-shutoff duration) has
// completed -- not merely that the link exists. That distinction matters:
// the gap between the two is seconds long, not milliseconds.
//
// CONNECTING begins once service discovery completes (VolcanoBleClient's
// ESP_GATTC_SEARCH_CMPL_EVT), not at the raw physical link coming up
// (ESP_GATTC_OPEN_EVT) -- see ADR-0009's Notes for why that earlier window
// is left as DISCONNECTED rather than a still-earlier CONNECTING.
enum class ConnectionState {
  DISCONNECTED,
  CONNECTING,
  READY,
};

// The Volcano's device state, per ADR-0009 -- the single authoritative copy
// ADR-0002 requires. Every temperature is Celsius, every duration seconds,
// matching the wire encoding decoded degrees/seconds documented in
// docs/protocol/. Device information strings are exempt from the
// disconnect-clears-unknown rule VolcanoDevice applies to everything else
// here: they are properties of the unit, not live state.
struct VolcanoState {
  DeviceValue<bool> heater;
  DeviceValue<bool> pump;
  DeviceValue<float> current_temperature_c;
  DeviceValue<float> target_temperature_c;
  DeviceValue<uint16_t> auto_shutoff_countdown_s;
  DeviceValue<uint16_t> auto_shutoff_duration_s;
  DeviceValue<uint8_t> led_brightness_percent;
  DeviceValue<bool> vibration;
  DeviceValue<bool> display_on_cooling;
  DeviceValue<bool> display_units_fahrenheit;
  DeviceValue<uint32_t> hours_of_operation;
  DeviceValue<uint16_t> minutes_of_operation;
  DeviceValue<std::string> firmware_version;
  DeviceValue<std::string> ble_firmware_version;
  DeviceValue<std::string> serial_number;
  DeviceValue<std::string> power_supply;
  DeviceValue<std::string> product_line;
};

class VolcanoBleClient;

// The Volcano abstraction layer (ADR-0002, ADR-0009). Owns the state model
// above, connection state, and the requested-versus-confirmed handling of
// writes. Has no BLE or ESP-IDF dependency and is not an ESPHome Component,
// so it is constructible and exercisable without any BLE hardware present
// -- a control interface can be built and tested against it directly.
class VolcanoDevice : public VolcanoBleClientObserver {
 public:
  // No protocol finding specifies how long to wait for a write to confirm;
  // every write path this depends on completes in well under a second
  // under healthy conditions. 5s is headroom over that, not a tuned figure.
  static constexpr uint32_t PENDING_WRITE_TIMEOUT_MS = 5000;

  // Exposed so a control interface can constrain its own input to what
  // CMD-001/002/003 confirm the device accepts. VolcanoBleClient is the
  // sole enforcement point for these bounds (ADR-0009); VolcanoDevice does
  // not re-check them itself.
  static constexpr float MIN_TARGET_TEMPERATURE_C = 40.0f;
  static constexpr float MAX_TARGET_TEMPERATURE_C = 230.0f;
  static constexpr uint16_t MIN_AUTO_SHUTOFF_DURATION_S = 60;
  static constexpr uint16_t MAX_AUTO_SHUTOFF_DURATION_S = 21600;
  static constexpr uint8_t MAX_LED_BRIGHTNESS_PERCENT = 100;

  // Not owned. Writes are forwarded here; must be set before any set_*()
  // call below is used.
  void set_ble_client(VolcanoBleClient *client) { this->ble_client_ = client; }

  // Defaults to esphome::millis() (set in the .cpp, which is the only
  // place this header's translation unit touches ESPHome's platform
  // layer); overridable so a host test can drive process() deterministically.
  void set_clock(std::function<uint32_t()> clock) { this->clock_ = std::move(clock); }

  // Fires on any change to connection state or any field's known-ness,
  // value, or requested() -- carries no identity of what changed.
  void add_on_state_callback(std::function<void()> &&callback);

  // Expires any pending write whose deadline has passed. Call periodically;
  // a no-op when nothing is outstanding.
  void process();

  ConnectionState connection_state() const { return this->connection_state_; }

  const DeviceValue<bool> &heater() const { return this->state_.heater; }
  const DeviceValue<bool> &pump() const { return this->state_.pump; }
  const DeviceValue<float> &current_temperature() const { return this->state_.current_temperature_c; }
  const DeviceValue<float> &target_temperature() const { return this->state_.target_temperature_c; }
  const DeviceValue<uint16_t> &auto_shutoff_countdown() const { return this->state_.auto_shutoff_countdown_s; }
  const DeviceValue<uint16_t> &auto_shutoff_duration() const { return this->state_.auto_shutoff_duration_s; }
  const DeviceValue<uint8_t> &led_brightness() const { return this->state_.led_brightness_percent; }
  const DeviceValue<bool> &vibration() const { return this->state_.vibration; }
  const DeviceValue<bool> &display_on_cooling() const { return this->state_.display_on_cooling; }
  const DeviceValue<bool> &display_units_fahrenheit() const { return this->state_.display_units_fahrenheit; }
  const DeviceValue<uint32_t> &hours_of_operation() const { return this->state_.hours_of_operation; }
  const DeviceValue<uint16_t> &minutes_of_operation() const { return this->state_.minutes_of_operation; }
  const DeviceValue<std::string> &firmware_version() const { return this->state_.firmware_version; }
  const DeviceValue<std::string> &ble_firmware_version() const { return this->state_.ble_firmware_version; }
  const DeviceValue<std::string> &serial_number() const { return this->state_.serial_number; }
  const DeviceValue<std::string> &power_supply() const { return this->state_.power_supply; }
  const DeviceValue<std::string> &product_line() const { return this->state_.product_line; }

  // Commands -- ADR-0009's interface. Requests, not authoritative state
  // changes (ADR-0002): none of these updates value() directly. Each is a
  // no-op, logged by the BLE communication layer, if no connection is
  // established or the value is outside the confirmed-accepted range. Each
  // is also a no-op, logged here, while a previous request for that same
  // field is still outstanding -- issuing a second write before the first
  // has resolved would let its eventual read-back/notification resolve
  // whichever request happens to be current at the time, rather than the
  // one it actually answers.
  void set_target_temperature(float celsius);
  void set_heater(bool on);
  void set_pump(bool on);
  void set_auto_shutoff_duration(uint16_t seconds);
  void set_led_brightness(uint8_t percent);
  void set_vibration(bool enabled);
  void set_display_on_cooling(bool enabled);
  void set_display_units_fahrenheit(bool fahrenheit);

  void on_connecting() override;
  void on_ready() override;
  void on_disconnected() override;
  void on_actuator_state(bool heater_on, bool pump_on) override;
  void on_current_temperature(bool has_reading, float celsius) override;
  void on_target_temperature(float celsius) override;
  void on_auto_shutoff_countdown(uint16_t seconds) override;
  void on_auto_shutoff_duration(uint16_t seconds) override;
  void on_led_brightness(uint8_t percent) override;
  void on_vibration(bool enabled) override;
  void on_display_register(bool display_on_cooling, bool display_units_fahrenheit) override;
  void on_hours_of_operation(uint32_t hours) override;
  void on_minutes_of_operation(uint16_t minutes) override;
  void on_firmware_version(const std::string &value) override;
  void on_ble_firmware_version(const std::string &value) override;
  void on_serial_number(const std::string &value) override;
  void on_power_supply(const std::string &value) override;
  void on_product_line(const std::string &value) override;
  void on_write_failed(VolcanoField field) override;
  void on_write_acked(VolcanoField field) override;

 private:
  // Updates a field from a device report: adopts the reported value always,
  // and resolves requested() if the report is trustworthy as this write's
  // own outcome -- a match always is (unambiguous regardless of source), a
  // mismatch only once write_acked_ confirms this write's own ATT-level
  // completion has actually been observed (STATE-013's silent-drop
  // signature; otherwise an unrelated report for the same field -- e.g.
  // STATE-009's temperature-change pulse on the display register -- would
  // look identical to a drop before the write has even reached the
  // device). Fires the state callback iff anything about the field actually
  // changed. `written_by` names the VolcanoField this report can confirm a
  // pending write against, so its timeout deadline and ack flag can be
  // cancelled too; omitted for fields nothing ever writes (current
  // temperature, the countdown, the runtime meter, device information).
  template<typename T>
  void update_value_(DeviceValue<T> &field, const T &new_value, optional<VolcanoField> written_by = nullopt);

  // Marks a field unknown and clears any pending request/deadline for it.
  // Returns whether anything changed; does not fire the callback itself --
  // on_connecting()/on_disconnected() change many fields at once and fire
  // a single callback for the whole transition, per add_on_state_callback()'s
  // "no field identity" contract.
  template<typename T> bool mark_unknown_(DeviceValue<T> &field);

  // Marks every field but the five device-information strings unknown, for
  // on_connecting()/on_disconnected(). Does not fire the callback; the
  // caller does, once, after also updating connection_state_.
  void mark_live_state_unknown_();

  // Records a pending write: sets requested() and its timeout deadline.
  // Called only once the BLE communication layer confirms it actually
  // issued the write (VolcanoBleClient's write_*() return false, and this
  // is skipped, when the value is out of range or nothing is connected).
  template<typename T> void set_requested_(VolcanoField field, DeviceValue<T> &value_field, const T &requested);

  // Clears requested() on a single field, if set, leaving value()
  // untouched. Returns whether anything changed.
  template<typename T> bool clear_requested_(DeviceValue<T> &field);

  // Dispatches to clear_requested_() for the state_ field a VolcanoField
  // names. Used by process() (timeout) and on_write_failed() (immediate
  // failure) -- both are "give up waiting for this write" outcomes.
  bool clear_requested_for_field_(VolcanoField field);

  uint32_t now_() const;

  void fire_callback_();

  ConnectionState connection_state_{ConnectionState::DISCONNECTED};
  VolcanoState state_;
  VolcanoBleClient *ble_client_{nullptr};
  std::function<uint32_t()> clock_;
  std::vector<std::function<void()>> callbacks_;
  // Sized from VolcanoField::COUNT rather than a hand-counted literal, so a
  // future field added to that enum can't silently leave this array too
  // small -- indexing it by an out-of-range VolcanoField would otherwise be
  // undefined behaviour rather than a compile error.
  std::array<optional<uint32_t>, static_cast<size_t>(VolcanoField::COUNT)> pending_deadlines_;

  // Whether the currently-outstanding write for each field (if any) has had
  // its own ATT-level completion observed yet (on_write_acked()). False from
  // the moment a request is made until that ack arrives; a mismatching
  // report for the field is only trusted as a silent drop once this is
  // true -- see update_value_(). Reset alongside pending_deadlines_.
  std::array<bool, static_cast<size_t>(VolcanoField::COUNT)> write_acked_{};
};

}  // namespace volcano
}  // namespace esphome
