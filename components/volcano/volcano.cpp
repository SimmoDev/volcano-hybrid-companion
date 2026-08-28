#include "volcano.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace volcano {

static const char *const TAG = "volcano";

namespace {

// sensor::Sensor and number::Number::publish_state() do not dedup, unlike
// switch_::Switch's (see publish_switch() below) -- so on_device_state_
// changed_() below, which fires on any field's change rather than only the
// one field a BLE callback used to scope a publish_state() call to, dedups
// itself. NAN is ESPHome's convention for "no valid reading"; has_state()
// gates the very first publish, since a coincidental match against an
// entity's un-published default must still publish once.
//
// Templated rather than overloaded for sensor::Sensor*/number::Number*
// separately: the two types share no common base with has_state()/state/
// publish_state(), but both satisfy this function's structural requirements
// identically, so one definition covers both without duplicating the body.
template<typename Entity> void publish_numeric(Entity *entity, bool known, float value) {
  if (entity == nullptr)
    return;
  float new_state = known ? value : NAN;
  bool changed = !entity->has_state() || std::isnan(new_state) != std::isnan(entity->state) ||
                 (!std::isnan(new_state) && new_state != entity->state);
  if (changed)
    entity->publish_state(new_state);
}

// text_sensor has no NAN equivalent, so unknown is set_has_state(false)
// directly -- the API's TextSensorStateResponse reports that as
// missing_state, unlike switch (see ADR-0009's Context).
void publish_text(text_sensor::TextSensor *entity, bool known, const std::string &value) {
  if (entity == nullptr)
    return;
  if (known) {
    if (!entity->has_state() || entity->state != value)
      entity->publish_state(value);
  } else if (entity->has_state()) {
    entity->set_has_state(false);
  }
}

// ADR-0009: a switch cannot express unknown -- ESPHome's switch entity has
// no missing_state concept at all -- so unknown means leaving it exactly as
// it last published rather than forcing it to any value. Switch::
// publish_state() dedups redundant calls internally, so no explicit
// "changed" check is needed here when known.
void publish_switch(switch_::Switch *entity, bool known, bool value) {
  if (entity == nullptr || !known)
    return;
  entity->publish_state(value);
}

}  // namespace

void VolcanoComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Volcano component...");
  this->ble_client_.set_client(this->parent());
  this->ble_client_.set_observer(&this->device_);
  this->ble_client_.set_node_state(&this->node_state);
  this->device_.set_ble_client(&this->ble_client_);
  this->device_.add_on_state_callback([this] { this->on_device_state_changed_(); });
  // Drives VolcanoDevice::process() (pending-write timeout expiry) off
  // ESPHome's central scheduler, which runs independently of any
  // component's own loop() -- so this component's loop stays disabled
  // below, since every other piece of it is purely event-driven from BLE
  // callbacks.
  this->set_interval("volcano_device_process", 1000, [this] { this->device_.process(); });
  this->disable_loop();
}

void VolcanoComponent::loop() {}

void VolcanoComponent::dump_config() {
  // Grouped by how each characteristic is reached, not by direction: the
  // settings characteristics (target temperature, vibration, the
  // display/units register) are two-way, so they appear under both a read
  // group and the write list below.
  ESP_LOGCONFIG(TAG, "Volcano:");
  ESP_LOGCONFIG(TAG, "  Subscribed (notify): status/flags register (CHAR-008), current temperature (CHAR-013),");
  ESP_LOGCONFIG(TAG, "    target temperature (CHAR-014), auto-shutoff countdown (CHAR-016),");
  ESP_LOGCONFIG(TAG, "    hours/minutes of operation (CHAR-022/023), vibration (CHAR-010),");
  ESP_LOGCONFIG(TAG, "    display/units register (CHAR-009).");
  ESP_LOGCONFIG(TAG, "  Read once per connection: auto-shutoff duration (CHAR-017), LED brightness (CHAR-015),");
  ESP_LOGCONFIG(TAG, "    firmware/BLE-firmware version (CHAR-005/006), serial number (CHAR-007),");
  ESP_LOGCONFIG(TAG, "    power supply (CHAR-024), product line (CHAR-025).");
  ESP_LOGCONFIG(TAG, "  Write: auto-shutoff duration (CHAR-017), %u-%u s; heater/pump on/off (CHAR-018-021);",
                VolcanoDevice::MIN_AUTO_SHUTOFF_DURATION_S, VolcanoDevice::MAX_AUTO_SHUTOFF_DURATION_S);
  ESP_LOGCONFIG(TAG, "    target temperature (CHAR-014), %.1f-%.1f C; LED brightness (CHAR-015), 0-%u%%;",
                VolcanoDevice::MIN_TARGET_TEMPERATURE_C, VolcanoDevice::MAX_TARGET_TEMPERATURE_C,
                VolcanoDevice::MAX_LED_BRIGHTNESS_PERCENT);
  ESP_LOGCONFIG(TAG, "    vibration (CHAR-010), display on cooling and display units (CHAR-009).");
}

void VolcanoComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                           esp_ble_gattc_cb_param_t *param) {
  this->ble_client_.gattc_event_handler(event, gattc_if, param);
}

void VolcanoComponent::set_target_temperature(float celsius) { this->device_.set_target_temperature(celsius); }

void VolcanoComponent::set_auto_shutoff_duration_seconds(uint16_t seconds) {
  this->device_.set_auto_shutoff_duration(seconds);
}

void VolcanoComponent::set_led_brightness_percent(uint8_t percent) { this->device_.set_led_brightness(percent); }

void VolcanoComponent::set_vibration(bool enabled) { this->device_.set_vibration(enabled); }

void VolcanoComponent::set_display_on_cooling(bool enabled) { this->device_.set_display_on_cooling(enabled); }

void VolcanoComponent::set_display_units_fahrenheit(bool fahrenheit) {
  this->device_.set_display_units_fahrenheit(fahrenheit);
}

void VolcanoComponent::set_heater(bool on) { this->device_.set_heater(on); }

void VolcanoComponent::set_pump(bool on) { this->device_.set_pump(on); }

void VolcanoComponent::on_device_state_changed_() {
  const auto &heater = this->device_.heater();
  publish_switch(this->heater_switch_, heater.is_known(), heater.value());

  const auto &pump = this->device_.pump();
  publish_switch(this->pump_switch_, pump.is_known(), pump.value());

  const auto &current_temp = this->device_.current_temperature();
  publish_numeric(this->current_temperature_sensor_, current_temp.is_known(), current_temp.value());

  const auto &target_temp = this->device_.target_temperature();
  publish_numeric(this->target_temperature_number_, target_temp.is_known(), target_temp.value());

  const auto &countdown = this->device_.auto_shutoff_countdown();
  publish_numeric(this->auto_shutoff_countdown_sensor_, countdown.is_known(), static_cast<float>(countdown.value()));

  // Published in minutes, the unit this entity carries -- CHAR-017 itself,
  // and VolcanoDevice::set_auto_shutoff_duration(), are seconds.
  const auto &duration = this->device_.auto_shutoff_duration();
  publish_numeric(this->auto_shutoff_duration_number_, duration.is_known(), duration.value() / 60.0f);

  const auto &led = this->device_.led_brightness();
  publish_numeric(this->led_brightness_number_, led.is_known(), static_cast<float>(led.value()));

  const auto &vibration = this->device_.vibration();
  publish_switch(this->vibration_switch_, vibration.is_known(), vibration.value());

  const auto &display_on_cooling = this->device_.display_on_cooling();
  publish_switch(this->display_on_cooling_switch_, display_on_cooling.is_known(), display_on_cooling.value());

  const auto &display_units = this->device_.display_units_fahrenheit();
  publish_switch(this->display_units_switch_, display_units.is_known(), display_units.value());

  const auto &hours = this->device_.hours_of_operation();
  publish_numeric(this->hours_sensor_, hours.is_known(), static_cast<float>(hours.value()));

  const auto &minutes = this->device_.minutes_of_operation();
  publish_numeric(this->minutes_sensor_, minutes.is_known(), static_cast<float>(minutes.value()));

  const auto &firmware = this->device_.firmware_version();
  publish_text(this->firmware_version_text_sensor_, firmware.is_known(), firmware.value());

  const auto &ble_firmware = this->device_.ble_firmware_version();
  publish_text(this->ble_firmware_version_text_sensor_, ble_firmware.is_known(), ble_firmware.value());

  const auto &serial = this->device_.serial_number();
  publish_text(this->serial_number_text_sensor_, serial.is_known(), serial.value());

  const auto &power_supply = this->device_.power_supply();
  publish_text(this->power_supply_text_sensor_, power_supply.is_known(), power_supply.value());

  const auto &product_line = this->device_.product_line();
  publish_text(this->product_line_text_sensor_, product_line.is_known(), product_line.value());

  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_state(this->device_.connection_state() == ConnectionState::READY);
}

// The entity's own min/max have already clamped `value` by the time this is
// reached, but they are configurable and so cannot be relied on as the
// safety bound -- VolcanoBleClient::write_target_temperature() re-checks
// against the range CMD-001 confirms accepted.
void VolcanoTargetTemperatureNumber::control(float value) { this->parent_->set_target_temperature(value); }

void VolcanoVibrationSwitch::write_state(bool state) { this->parent_->set_vibration(state); }

void VolcanoDisplayOnCoolingSwitch::write_state(bool state) { this->parent_->set_display_on_cooling(state); }

void VolcanoDisplayUnitsSwitch::write_state(bool state) { this->parent_->set_display_units_fahrenheit(state); }

void VolcanoLedBrightnessNumber::control(float value) {
  long percent = lroundf(value);
  if (percent < 0 || percent > UINT8_MAX) {
    ESP_LOGW(TAG, "Ignoring LED brightness of %.0f: outside the encodable range", value);
    return;
  }
  this->parent_->set_led_brightness_percent(static_cast<uint8_t>(percent));
}

// Minutes in, seconds out -- see VolcanoAutoShutoffDurationNumber in
// volcano.h for why this entity carries minutes. Rounds to the nearest
// second, not the nearest minute: CHAR-017's wire encoding is seconds, and
// rounding minutes first would silently discard a `step` configured finer
// than 1.0 (e.g. 0.5 minutes, still within CMD-003's 60-21600s range).
void VolcanoAutoShutoffDurationNumber::control(float value) {
  long seconds = lroundf(value * 60.0f);
  if (seconds < 0 || seconds > UINT16_MAX) {
    ESP_LOGW(TAG, "Ignoring auto-shutoff duration of %.0f min: outside the encodable range", value);
    return;
  }
  this->parent_->set_auto_shutoff_duration_seconds(static_cast<uint16_t>(seconds));
}

void VolcanoHeaterSwitch::write_state(bool state) { this->parent_->set_heater(state); }

void VolcanoPumpSwitch::write_state(bool state) { this->parent_->set_pump(state); }

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
