#include "volcano_device.h"
#include "volcano_ble_client.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace volcano {

static const char *const TAG = "volcano.device";

void VolcanoDevice::add_on_state_callback(std::function<void()> &&callback) {
  this->callbacks_.push_back(std::move(callback));
}

void VolcanoDevice::fire_callback_() {
  for (auto &callback : this->callbacks_)
    callback();
}

uint32_t VolcanoDevice::now_() const { return this->clock_ ? this->clock_() : esphome::millis(); }

template<typename T> bool VolcanoDevice::mark_unknown_(DeviceValue<T> &field) {
  bool changed = field.known_ || field.requested_.has_value();
  field.known_ = false;
  field.value_ = T{};
  field.requested_.reset();
  return changed;
}

void VolcanoDevice::mark_live_state_unknown_() {
  this->mark_unknown_(this->state_.heater);
  this->mark_unknown_(this->state_.pump);
  this->mark_unknown_(this->state_.current_temperature_c);
  this->mark_unknown_(this->state_.target_temperature_c);
  this->mark_unknown_(this->state_.auto_shutoff_countdown_s);
  this->mark_unknown_(this->state_.auto_shutoff_duration_s);
  this->mark_unknown_(this->state_.led_brightness_percent);
  this->mark_unknown_(this->state_.vibration);
  this->mark_unknown_(this->state_.display_on_cooling);
  this->mark_unknown_(this->state_.display_units_fahrenheit);
  this->mark_unknown_(this->state_.hours_of_operation);
  this->mark_unknown_(this->state_.minutes_of_operation);
  // Device information strings are deliberately left alone -- properties of
  // the unit, not live state (ADR-0009).
  for (auto &deadline : this->pending_deadlines_)
    deadline.reset();
}

template<typename T>
void VolcanoDevice::update_value_(DeviceValue<T> &field, const T &new_value, optional<VolcanoField> written_by) {
  bool changed = !field.known_ || !(field.value_ == new_value);
  field.value_ = new_value;
  field.known_ = true;
  if (field.requested_.has_value()) {
    // Resolved either way: a match is a plain confirm, a mismatch is
    // STATE-013's silent-drop signature -- the device answered with
    // something other than what this just requested. value_ above already
    // reflects the truth the device reported either way.
    field.requested_.reset();
    changed = true;
    if (written_by.has_value())
      this->pending_deadlines_[static_cast<size_t>(*written_by)].reset();
  }
  if (changed)
    this->fire_callback_();
}

template<typename T> bool VolcanoDevice::clear_requested_(DeviceValue<T> &field) {
  if (!field.requested_.has_value())
    return false;
  field.requested_.reset();
  return true;
}

bool VolcanoDevice::clear_requested_for_field_(VolcanoField field) {
  this->pending_deadlines_[static_cast<size_t>(field)].reset();
  switch (field) {
    case VolcanoField::TARGET_TEMPERATURE:
      return this->clear_requested_(this->state_.target_temperature_c);
    case VolcanoField::AUTO_SHUTOFF_DURATION:
      return this->clear_requested_(this->state_.auto_shutoff_duration_s);
    case VolcanoField::LED_BRIGHTNESS:
      return this->clear_requested_(this->state_.led_brightness_percent);
    case VolcanoField::HEATER:
      return this->clear_requested_(this->state_.heater);
    case VolcanoField::PUMP:
      return this->clear_requested_(this->state_.pump);
    case VolcanoField::VIBRATION:
      return this->clear_requested_(this->state_.vibration);
    case VolcanoField::DISPLAY_ON_COOLING:
      return this->clear_requested_(this->state_.display_on_cooling);
    case VolcanoField::DISPLAY_UNITS_FAHRENHEIT:
      return this->clear_requested_(this->state_.display_units_fahrenheit);
    case VolcanoField::COUNT:
      break;  // Sentinel only, never a real field; satisfies -Wswitch.
  }
  return false;
}

template<typename T>
void VolcanoDevice::set_requested_(VolcanoField field, DeviceValue<T> &value_field, const T &requested) {
  value_field.requested_ = requested;
  this->pending_deadlines_[static_cast<size_t>(field)] = this->now_() + PENDING_WRITE_TIMEOUT_MS;
  this->fire_callback_();
}

void VolcanoDevice::process() {
  uint32_t now = this->now_();
  bool changed = false;
  for (size_t i = 0; i < this->pending_deadlines_.size(); i++) {
    if (this->pending_deadlines_[i].has_value() && now >= *this->pending_deadlines_[i]) {
      if (this->clear_requested_for_field_(static_cast<VolcanoField>(i)))
        changed = true;
    }
  }
  if (changed)
    this->fire_callback_();
}

void VolcanoDevice::on_connecting() {
  this->connection_state_ = ConnectionState::CONNECTING;
  this->mark_live_state_unknown_();
  this->fire_callback_();
}

void VolcanoDevice::on_ready() {
  this->connection_state_ = ConnectionState::READY;
  this->fire_callback_();
}

void VolcanoDevice::on_disconnected() {
  this->connection_state_ = ConnectionState::DISCONNECTED;
  this->mark_live_state_unknown_();
  this->fire_callback_();
}

void VolcanoDevice::on_actuator_state(bool heater_on, bool pump_on) {
  this->update_value_(this->state_.heater, heater_on, VolcanoField::HEATER);
  this->update_value_(this->state_.pump, pump_on, VolcanoField::PUMP);
}

void VolcanoDevice::on_current_temperature(bool has_reading, float celsius) {
  if (!has_reading) {
    // STATE-012: below 40 degC with the heater off, the device is not
    // reporting a real reading -- always unknown, regardless of connection
    // state or whatever was last known.
    if (this->mark_unknown_(this->state_.current_temperature_c))
      this->fire_callback_();
    return;
  }
  this->update_value_(this->state_.current_temperature_c, celsius);
}

void VolcanoDevice::on_target_temperature(float celsius) {
  this->update_value_(this->state_.target_temperature_c, celsius, VolcanoField::TARGET_TEMPERATURE);
}

void VolcanoDevice::on_auto_shutoff_countdown(uint16_t seconds) {
  this->update_value_(this->state_.auto_shutoff_countdown_s, seconds);
}

void VolcanoDevice::on_auto_shutoff_duration(uint16_t seconds) {
  this->update_value_(this->state_.auto_shutoff_duration_s, seconds, VolcanoField::AUTO_SHUTOFF_DURATION);
}

void VolcanoDevice::on_led_brightness(uint8_t percent) {
  this->update_value_(this->state_.led_brightness_percent, percent, VolcanoField::LED_BRIGHTNESS);
}

void VolcanoDevice::on_vibration(bool enabled) {
  this->update_value_(this->state_.vibration, enabled, VolcanoField::VIBRATION);
}

void VolcanoDevice::on_display_register(bool display_on_cooling, bool display_units_fahrenheit) {
  this->update_value_(this->state_.display_on_cooling, display_on_cooling, VolcanoField::DISPLAY_ON_COOLING);
  this->update_value_(this->state_.display_units_fahrenheit, display_units_fahrenheit,
                      VolcanoField::DISPLAY_UNITS_FAHRENHEIT);
}

void VolcanoDevice::on_hours_of_operation(uint32_t hours) {
  this->update_value_(this->state_.hours_of_operation, hours);
}

void VolcanoDevice::on_minutes_of_operation(uint16_t minutes) {
  this->update_value_(this->state_.minutes_of_operation, minutes);
}

void VolcanoDevice::on_firmware_version(const std::string &value) {
  this->update_value_(this->state_.firmware_version, value);
}

void VolcanoDevice::on_ble_firmware_version(const std::string &value) {
  this->update_value_(this->state_.ble_firmware_version, value);
}

void VolcanoDevice::on_serial_number(const std::string &value) {
  this->update_value_(this->state_.serial_number, value);
}

void VolcanoDevice::on_power_supply(const std::string &value) { this->update_value_(this->state_.power_supply, value); }

void VolcanoDevice::on_product_line(const std::string &value) { this->update_value_(this->state_.product_line, value); }

void VolcanoDevice::on_write_failed(VolcanoField field) {
  if (this->clear_requested_for_field_(field))
    this->fire_callback_();
}

void VolcanoDevice::set_target_temperature(float celsius) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_target_temperature() called with no BLE client attached");
    return;
  }
  // Rounded to CMD-001's 0.1 degC wire granularity, so a successful
  // write's read-back (STATE-013) -- computed the same way on the decode
  // side -- compares equal to what was requested here.
  float rounded = std::lroundf(celsius * 10.0f) / 10.0f;
  if (this->ble_client_->write_target_temperature(rounded))
    this->set_requested_(VolcanoField::TARGET_TEMPERATURE, this->state_.target_temperature_c, rounded);
}

void VolcanoDevice::set_heater(bool on) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_heater() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_heater(on))
    this->set_requested_(VolcanoField::HEATER, this->state_.heater, on);
}

void VolcanoDevice::set_pump(bool on) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_pump() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_pump(on))
    this->set_requested_(VolcanoField::PUMP, this->state_.pump, on);
}

void VolcanoDevice::set_auto_shutoff_duration(uint16_t seconds) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_auto_shutoff_duration() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_auto_shutoff_duration(seconds))
    this->set_requested_(VolcanoField::AUTO_SHUTOFF_DURATION, this->state_.auto_shutoff_duration_s, seconds);
}

void VolcanoDevice::set_led_brightness(uint8_t percent) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_led_brightness() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_led_brightness(percent))
    this->set_requested_(VolcanoField::LED_BRIGHTNESS, this->state_.led_brightness_percent, percent);
}

void VolcanoDevice::set_vibration(bool enabled) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_vibration() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_vibration(enabled))
    this->set_requested_(VolcanoField::VIBRATION, this->state_.vibration, enabled);
}

void VolcanoDevice::set_display_on_cooling(bool enabled) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_display_on_cooling() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_display_on_cooling(enabled))
    this->set_requested_(VolcanoField::DISPLAY_ON_COOLING, this->state_.display_on_cooling, enabled);
}

void VolcanoDevice::set_display_units_fahrenheit(bool fahrenheit) {
  if (this->ble_client_ == nullptr) {
    ESP_LOGW(TAG, "set_display_units_fahrenheit() called with no BLE client attached");
    return;
  }
  if (this->ble_client_->write_display_units_fahrenheit(fahrenheit))
    this->set_requested_(VolcanoField::DISPLAY_UNITS_FAHRENHEIT, this->state_.display_units_fahrenheit, fahrenheit);
}

}  // namespace volcano
}  // namespace esphome
