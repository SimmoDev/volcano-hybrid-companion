#include "volcano_ble_client.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace volcano {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const TAG = "volcano.ble_client";

// Service UUIDs, per docs/protocol/gatt-services.md. Characteristics below
// are resolved by (service, characteristic) UUID pair, never by handle:
// several characteristics on this device share an identical one-byte
// read/write shape, and writing to the wrong one actuates real hardware --
// see docs/protocol/README.md's "What these findings apply to" section.
static const char *const SETTINGS_SERVICE_UUID = "10100000-5354-4f52-5a26-4249434b454c";  // SVC-005
static const char *const CONTROL_SERVICE_UUID = "10110000-5354-4f52-5a26-4249434b454c";   // SVC-006

// CHAR-008: status/flags register (SVC-005).
static const char *const STATUS_CHARACTERISTIC_UUID = "1010000c-5354-4f52-5a26-4249434b454c";
// CHAR-016: auto-shutoff countdown (SVC-006). Note the "10110" prefix vs.
// CHAR-008's "10100" above -- easy to mistype, deliberately spelled out
// here rather than reused.
static const char *const COUNTDOWN_CHARACTERISTIC_UUID = "1011000c-5354-4f52-5a26-4249434b454c";
// CHAR-017: auto-shutoff duration (SVC-006).
static const char *const DURATION_CHARACTERISTIC_UUID = "1011000d-5354-4f52-5a26-4249434b454c";
// CHAR-013: current (actual) temperature (SVC-006).
static const char *const CURRENT_TEMP_CHARACTERISTIC_UUID = "10110001-5354-4f52-5a26-4249434b454c";
// CHAR-014: target temperature (SVC-006).
static const char *const TARGET_TEMP_CHARACTERISTIC_UUID = "10110003-5354-4f52-5a26-4249434b454c";
// CHAR-018/CMD-006: heater on trigger (SVC-006).
static const char *const HEATER_ON_CHARACTERISTIC_UUID = "1011000f-5354-4f52-5a26-4249434b454c";
// CHAR-019/CMD-007: heater off trigger (SVC-006).
static const char *const HEATER_OFF_CHARACTERISTIC_UUID = "10110010-5354-4f52-5a26-4249434b454c";
// CHAR-020/CMD-008: pump on trigger (SVC-006).
static const char *const PUMP_ON_CHARACTERISTIC_UUID = "10110013-5354-4f52-5a26-4249434b454c";
// CHAR-021/CMD-009: pump off trigger (SVC-006).
static const char *const PUMP_OFF_CHARACTERISTIC_UUID = "10110014-5354-4f52-5a26-4249434b454c";
// Device information, all on SVC-005. CHAR-007, CHAR-024 and CHAR-025 are
// Read/Write on the device; this component only ever reads them.
// CHAR-005: firmware version (STATE-003).
static const char *const FIRMWARE_VERSION_CHARACTERISTIC_UUID = "10100003-5354-4f52-5a26-4249434b454c";
// CHAR-006: firmware BLE version (STATE-004).
static const char *const BLE_FIRMWARE_VERSION_CHARACTERISTIC_UUID = "10100004-5354-4f52-5a26-4249434b454c";
// CHAR-024: power supply rating.
static const char *const POWER_SUPPLY_CHARACTERISTIC_UUID = "10100006-5354-4f52-5a26-4249434b454c";
// CHAR-025: product line name.
static const char *const PRODUCT_LINE_CHARACTERISTIC_UUID = "10100007-5354-4f52-5a26-4249434b454c";
// CHAR-007: serial number (STATE-002). Unique per unit.
static const char *const SERIAL_NUMBER_CHARACTERISTIC_UUID = "10100008-5354-4f52-5a26-4249434b454c";
// CHAR-022: hours of operation (STATE-001), on SVC-006. Notify-capable,
// unlike the device information above, so it is subscribed rather than
// read once: it advances live while the heater is on.
static const char *const HOURS_CHARACTERISTIC_UUID = "10110015-5354-4f52-5a26-4249434b454c";
// CHAR-023: minutes of operation (STATE-006), on SVC-006.
static const char *const MINUTES_CHARACTERISTIC_UUID = "10110016-5354-4f52-5a26-4249434b454c";
// CHAR-015/CMD-002: LED brightness (SVC-006).
static const char *const LED_BRIGHTNESS_CHARACTERISTIC_UUID = "10110005-5354-4f52-5a26-4249434b454c";
// CHAR-010/CMD-004: vibration setting (SVC-005). Note the "10100" prefix:
// this is a settings-service register, not one of the SVC-006 controls.
static const char *const VIBRATION_CHARACTERISTIC_UUID = "1010000e-5354-4f52-5a26-4249434b454c";
// CHAR-009/CMD-005: display-on-cooling and units register (SVC-005).
static const char *const DISPLAY_REGISTER_CHARACTERISTIC_UUID = "1010000d-5354-4f52-5a26-4249434b454c";

// STATE-008: bit 5 is set whenever the heater is on, clear when off.
static const uint16_t STATUS_BIT_HEATER_ON = 0x0020;
// STATE-008: bit 12 marks the pump specifically. Bit 13 is also pulsed by
// the vibration alert for about a second, so it must not be used to detect
// the pump.
static const uint16_t STATUS_BIT_PUMP_ON = 0x1000;

// CMD-003: the auto-shutoff duration range confirmed accepted. The floor is
// the lowest value verified read back unchanged, loaded at the next arming,
// and honoured through to an actual expiry; below it is unverified -- 0 in
// particular may mean "disabled" on this device. The ceiling is the top of
// the official app's own UI range, with writes at that end captured; above
// it is untested. write_auto_shutoff_duration() refuses both rather than
// writing an untested value (ADR-0005).
static const uint16_t MIN_AUTO_SHUTOFF_DURATION_SECONDS = 60;
static const uint16_t MAX_AUTO_SHUTOFF_DURATION_SECONDS = 21600;

// CMD-001: the official app's UI spans 40.0-230.0 degC (400-2300 in this
// characteristic's deci-degrees-Celsius encoding); that is the only range
// Confirmed accepted. What the device does with a value outside it is
// Unknown, so write_target_temperature() refuses them rather than writing
// an untested value (ADR-0005).
static const uint16_t MIN_TARGET_TEMPERATURE_DECIDEGREES = 400;
static const uint16_t MAX_TARGET_TEMPERATURE_DECIDEGREES = 2300;

// CMD-002: the scale the LED brightness characteristic is Confirmed to
// use, both written and read back. 0 is a legitimate value rather than one
// to guard against -- it switches the display off entirely rather than
// dimming it, and any non-zero write restores it at the level written --
// so it is allowed, and only values above the scale are refused.
static const uint8_t MAX_LED_BRIGHTNESS_PERCENT = 100;

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
static const uint32_t VIBRATION_BIT_DISABLED = 0x0400;

// CHAR-009/CMD-005: the display-on-cooling bit within its register, with the
// same inverted polarity as the vibration bit above -- clear means the
// device shows current temperature while cooling, set means it shows none.
//
// The register carries more than this: the Celsius/Fahrenheit units bit
// (STATE-010) and a bit that pulses on every 1 degC change of current
// temperature (STATE-009), plus bits still unidentified. None of those is
// decoded, and the mask-and-action write form names only the bit below, so
// writing this setting cannot disturb them.
static const uint32_t DISPLAY_ON_COOLING_BIT_DISABLED = 0x1000;

// STATE-010: the display units bit on the same register. Its polarity is
// the opposite way round to the two settings above -- set means Fahrenheit,
// clear means Celsius, with no inversion -- which is exactly the trap the
// setting-bit note in docs/protocol/commands.md warns about: the inversion
// is a property of those two settings, not of the register.
static const uint32_t DISPLAY_UNITS_BIT_FAHRENHEIT = 0x0200;

// esp_ble_gattc_register_for_notify() is asynchronous: on success, completion
// -- whether the registration itself then succeeds or fails -- always arrives
// later as ESP_GATTC_REG_FOR_NOTIFY_EVT. A non-OK return here instead means
// the request was never even enqueued (e.g. the BT stack's internal message
// queue is full), so no such event will ever follow for this handle. Returns
// whether an event can be expected, so the caller knows whether to count this
// subscription in pending_subscriptions_ at all -- counting one that will
// never be decremented would stall this connection at CONNECTING forever.
static bool subscribe(esphome::ble_client::BLEClient *client, uint16_t handle, const char *name) {
  auto status = esp_ble_gattc_register_for_notify(client->get_gattc_if(), client->get_remote_bda(), handle);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify(%s) failed, status=%d", name, status);
    return false;
  }
  return true;
}

void VolcanoBleClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                           esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      // ADR-0007: device state is marked unknown while disconnected.
      this->status_handle_ = 0;
      this->countdown_handle_ = 0;
      this->duration_handle_ = 0;
      this->current_temp_handle_ = 0;
      this->target_temp_handle_ = 0;
      this->heater_on_handle_ = 0;
      this->heater_off_handle_ = 0;
      this->pump_on_handle_ = 0;
      this->pump_off_handle_ = 0;
      this->firmware_version_handle_ = 0;
      this->ble_firmware_version_handle_ = 0;
      this->serial_number_handle_ = 0;
      this->power_supply_handle_ = 0;
      this->product_line_handle_ = 0;
      this->hours_handle_ = 0;
      this->minutes_handle_ = 0;
      this->led_brightness_handle_ = 0;
      this->vibration_handle_ = 0;
      this->display_register_handle_ = 0;
      this->display_on_cooling_log_state_ = -1;
      // Any write queued here will never get its ESP_GATTC_WRITE_CHAR_EVT
      // now the link is gone; leaving it would let a future connection's
      // first completion on this handle pop a stale entry instead of its
      // own.
      this->display_register_pending_writes_.clear();
      this->pending_subscriptions_ = 0;
      this->static_reads_.reset();
      this->static_sweep_done_ = false;
      ESP_LOGI(TAG, "Disconnected; heater/pump/countdown/temperature state unknown");
      if (this->observer_ != nullptr)
        this->observer_->on_disconnected();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        // ESPHome's own esp32_ble_client::BLEClientBase handles this same
        // event unconditionally: it advances the *parent* client's own
        // state to ESTABLISHED regardless of status, but that call goes
        // through set_state_internal_(), which bypasses the virtual
        // set_state() this node's node_state is only ever updated through
        // (ble_client::BLEClient::set_state() is what propagates a state
        // change to every node). So on this path node_state is left at
        // whatever OPEN_EVT set it (CONNECTED) and never reaches
        // ESTABLISHED, while the physical link stays up -- with nothing to
        // retry it, since esp32_ble_tracker only offers a fresh connection
        // attempt once this device is next seen advertising as
        // unconnected. Force a clean disconnect so that reconnect loop
        // actually runs, rather than leaving the component stuck at
        // CONNECTING indefinitely.
        ESP_LOGW(TAG, "Service discovery failed, status=%d; disconnecting to retry", param->search_cmpl.status);
        this->client_->disconnect();
        break;
      }
      if (this->observer_ != nullptr)
        this->observer_->on_connecting();

      auto settings_service = espbt::ESPBTUUID::from_raw(SETTINGS_SERVICE_UUID);
      auto control_service = espbt::ESPBTUUID::from_raw(CONTROL_SERVICE_UUID);

      this->pending_subscriptions_ = 0;
      // Reset defensively even though ESP_GATTC_DISCONNECT_EVT already does
      // this -- this path is the one thing that can start a static-read
      // sweep, so it is the one that must not find the flag left set.
      this->static_sweep_done_ = false;

      auto *status_chr =
          this->client_->get_characteristic(settings_service, espbt::ESPBTUUID::from_raw(STATUS_CHARACTERISTIC_UUID));
      if (status_chr == nullptr) {
        ESP_LOGW(TAG, "Status/flags register (CHAR-008) not found on device");
      } else {
        this->status_handle_ = status_chr->handle;
        // CONN-002: nothing is pushed on subscribing, so the initial value
        // is read explicitly once subscription completes -- see the
        // ESP_GATTC_REG_FOR_NOTIFY_EVT case below.
        if (subscribe(this->client_, this->status_handle_, "status/flags register"))
          this->pending_subscriptions_++;
      }

      auto *vibration_chr = this->client_->get_characteristic(
          settings_service, espbt::ESPBTUUID::from_raw(VIBRATION_CHARACTERISTIC_UUID));
      if (vibration_chr == nullptr) {
        ESP_LOGW(TAG, "Vibration setting (CHAR-010) not found on device");
      } else {
        // Read/Write/Notify: subscribed like the live values rather than
        // read once, so a change made elsewhere is picked up.
        this->vibration_handle_ = vibration_chr->handle;
        if (subscribe(this->client_, this->vibration_handle_, "vibration setting"))
          this->pending_subscriptions_++;
      }

      auto *display_register_chr = this->client_->get_characteristic(
          settings_service, espbt::ESPBTUUID::from_raw(DISPLAY_REGISTER_CHARACTERISTIC_UUID));
      if (display_register_chr == nullptr) {
        ESP_LOGW(TAG, "Display/units register (CHAR-009) not found on device");
      } else {
        this->display_register_handle_ = display_register_chr->handle;
        if (subscribe(this->client_, this->display_register_handle_, "display/units register"))
          this->pending_subscriptions_++;
      }

      auto *countdown_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(COUNTDOWN_CHARACTERISTIC_UUID));
      if (countdown_chr == nullptr) {
        ESP_LOGW(TAG, "Auto-shutoff countdown (CHAR-016) not found on device");
      } else {
        this->countdown_handle_ = countdown_chr->handle;
        if (subscribe(this->client_, this->countdown_handle_, "auto-shutoff countdown"))
          this->pending_subscriptions_++;
      }

      auto *duration_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(DURATION_CHARACTERISTIC_UUID));
      if (duration_chr == nullptr) {
        ESP_LOGW(TAG, "Auto-shutoff duration (CHAR-017) not found on device");
      } else {
        // Read/Write only, no Notify -- resolved here for
        // write_auto_shutoff_duration() below, not subscribed, and so not
        // counted in pending_subscriptions_.
        this->duration_handle_ = duration_chr->handle;
      }

      auto *current_temp_chr = this->client_->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(CURRENT_TEMP_CHARACTERISTIC_UUID));
      if (current_temp_chr == nullptr) {
        ESP_LOGW(TAG, "Current temperature (CHAR-013) not found on device");
      } else {
        this->current_temp_handle_ = current_temp_chr->handle;
        if (subscribe(this->client_, this->current_temp_handle_, "current temperature"))
          this->pending_subscriptions_++;
      }

      auto *target_temp_chr = this->client_->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(TARGET_TEMP_CHARACTERISTIC_UUID));
      if (target_temp_chr == nullptr) {
        ESP_LOGW(TAG, "Target temperature (CHAR-014) not found on device");
      } else {
        this->target_temp_handle_ = target_temp_chr->handle;
        if (subscribe(this->client_, this->target_temp_handle_, "target temperature"))
          this->pending_subscriptions_++;
      }

      auto *hours_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(HOURS_CHARACTERISTIC_UUID));
      if (hours_chr == nullptr) {
        ESP_LOGW(TAG, "Hours of operation (CHAR-022) not found on device");
      } else {
        this->hours_handle_ = hours_chr->handle;
        if (subscribe(this->client_, this->hours_handle_, "hours of operation"))
          this->pending_subscriptions_++;
      }

      auto *minutes_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(MINUTES_CHARACTERISTIC_UUID));
      if (minutes_chr == nullptr) {
        ESP_LOGW(TAG, "Minutes of operation (CHAR-023) not found on device");
      } else {
        this->minutes_handle_ = minutes_chr->handle;
        if (subscribe(this->client_, this->minutes_handle_, "minutes of operation"))
          this->pending_subscriptions_++;
      }

      // CHAR-018 through CHAR-021: one-byte Read/Write trigger
      // characteristics (CMD-006 through CMD-009), not Notify-capable, so
      // resolved here like the duration characteristic above and not
      // counted in pending_subscriptions_.
      auto *heater_on_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(HEATER_ON_CHARACTERISTIC_UUID));
      if (heater_on_chr == nullptr) {
        ESP_LOGW(TAG, "Heater on trigger (CHAR-018) not found on device");
      } else {
        this->heater_on_handle_ = heater_on_chr->handle;
      }

      auto *heater_off_chr = this->client_->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(HEATER_OFF_CHARACTERISTIC_UUID));
      if (heater_off_chr == nullptr) {
        ESP_LOGW(TAG, "Heater off trigger (CHAR-019) not found on device");
      } else {
        this->heater_off_handle_ = heater_off_chr->handle;
      }

      auto *pump_on_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(PUMP_ON_CHARACTERISTIC_UUID));
      if (pump_on_chr == nullptr) {
        ESP_LOGW(TAG, "Pump on trigger (CHAR-020) not found on device");
      } else {
        this->pump_on_handle_ = pump_on_chr->handle;
      }

      auto *pump_off_chr =
          this->client_->get_characteristic(control_service, espbt::ESPBTUUID::from_raw(PUMP_OFF_CHARACTERISTIC_UUID));
      if (pump_off_chr == nullptr) {
        ESP_LOGW(TAG, "Pump off trigger (CHAR-021) not found on device");
      } else {
        this->pump_off_handle_ = pump_off_chr->handle;
      }

      auto *led_brightness_chr = this->client_->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(LED_BRIGHTNESS_CHARACTERISTIC_UUID));
      if (led_brightness_chr == nullptr) {
        ESP_LOGW(TAG, "LED brightness (CHAR-015) not found on device");
      } else {
        // Read/Write with no notify, so resolved here and read once per
        // connection by the static read queue rather than subscribed.
        this->led_brightness_handle_ = led_brightness_chr->handle;
      }

      // Device information (SVC-005), none of it notify-capable, so all of
      // it is read once per connection by the static read queue rather than
      // subscribed. A missing one is logged at debug rather than warning:
      // unlike the characteristics above, nothing depends on them.
      struct {
        const char *uuid;
        uint16_t *handle;
        const char *name;
      } device_info[] = {
          {FIRMWARE_VERSION_CHARACTERISTIC_UUID, &this->firmware_version_handle_, "Firmware version (CHAR-005)"},
          {BLE_FIRMWARE_VERSION_CHARACTERISTIC_UUID, &this->ble_firmware_version_handle_,
           "Firmware BLE version (CHAR-006)"},
          {SERIAL_NUMBER_CHARACTERISTIC_UUID, &this->serial_number_handle_, "Serial number (CHAR-007)"},
          {POWER_SUPPLY_CHARACTERISTIC_UUID, &this->power_supply_handle_, "Power supply rating (CHAR-024)"},
          {PRODUCT_LINE_CHARACTERISTIC_UUID, &this->product_line_handle_, "Product line name (CHAR-025)"},
      };
      for (auto &entry : device_info) {
        auto *chr = this->client_->get_characteristic(settings_service, espbt::ESPBTUUID::from_raw(entry.uuid));
        if (chr == nullptr) {
          ESP_LOGD(TAG, "%s not found on device", entry.name);
        } else {
          *entry.handle = chr->handle;
        }
      }

      // Nothing to subscribe to: nothing else will ever drive node_state to
      // ESTABLISHED for this connection, so declare it done here instead of
      // leaving the parent waiting on a subscription that will never arrive.
      if (this->pending_subscriptions_ == 0) {
        if (this->node_state_ != nullptr)
          *this->node_state_ = espbt::ClientState::ESTABLISHED;
        this->queue_static_reads_();
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      uint16_t handle = param->reg_for_notify.handle;
      if (handle != this->status_handle_ && handle != this->countdown_handle_ && handle != this->current_temp_handle_ &&
          handle != this->target_temp_handle_ && handle != this->hours_handle_ && handle != this->minutes_handle_ &&
          handle != this->vibration_handle_ && handle != this->display_register_handle_)
        break;

      // node_state must not become ESTABLISHED until every subscription
      // issued in ESP_GATTC_SEARCH_CMPL_EVT above has been accounted for --
      // see the pending_subscriptions_ comment in volcano_ble_client.h for
      // why an early ESTABLISHED here is a use-after-free, not just a race.
      if (this->pending_subscriptions_ > 0)
        this->pending_subscriptions_--;
      if (this->pending_subscriptions_ == 0) {
        if (this->node_state_ != nullptr)
          *this->node_state_ = espbt::ClientState::ESTABLISHED;
        // The characteristics with no notify are read here rather than
        // subscribed -- once every subscription has settled, so this does
        // not contend with the reads they each issue below.
        this->queue_static_reads_();
      }

      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Subscribing to handle 0x%04x failed, status=%d", handle, param->reg_for_notify.status);
        break;
      }
      auto status = esp_ble_gattc_read_char(this->client_->get_gattc_if(), this->client_->get_conn_id(), handle,
                                            ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_read_char(0x%04x) failed, status=%d", handle, status);
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      // Whether this completes the static read currently in flight, advancing
      // the queue past it if so. Noted before anything else so a failed read
      // still advances the queue rather than stalling every read behind it. A
      // duration read-back after a write does not match, since the queue has
      // finished by then.
      bool completes_static_read = this->static_reads_.advance_if_current(param->read.handle);

      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Reading handle 0x%04x failed, status=%d", param->read.handle, param->read.status);
        if (completes_static_read)
          this->issue_next_static_read_();
        break;
      }
      if (param->read.handle == this->status_handle_) {
        this->decode_status_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->countdown_handle_) {
        this->decode_countdown_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->duration_handle_) {
        // Read-back after write_auto_shutoff_duration()'s write, to confirm
        // the raw value stuck at the protocol level -- separate from
        // whether it is honoured at the next arming, per STATE-005
        // (ADR-0002 requires confirming device state from the device
        // itself rather than trusting the ATT write response alone).
        if (param->read.value_len < 2) {
          ESP_LOGW(TAG, "Auto-shutoff duration value too short (%u bytes)", param->read.value_len);
        } else {
          uint16_t seconds = encode_uint16(param->read.value[1], param->read.value[0]);
          ESP_LOGI(TAG, "Auto-shutoff duration: %u s", seconds);
          if (this->observer_ != nullptr)
            this->observer_->on_auto_shutoff_duration(seconds);
        }
      } else if (param->read.handle == this->current_temp_handle_) {
        this->decode_current_temperature_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->target_temp_handle_) {
        this->decode_target_temperature_(param->read.value, param->read.value_len, true);
      } else if (param->read.handle == this->display_register_handle_) {
        this->decode_display_register_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->vibration_handle_) {
        this->decode_vibration_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->led_brightness_handle_) {
        // CMD-002: 2-byte value, only the low byte significant.
        if (param->read.value_len < 2) {
          ESP_LOGW(TAG, "LED brightness value too short (%u bytes)", param->read.value_len);
        } else {
          uint16_t percent = encode_uint16(param->read.value[1], param->read.value[0]);
          ESP_LOGI(TAG, "LED brightness: %u%%", percent);
          if (this->observer_ != nullptr)
            this->observer_->on_led_brightness(static_cast<uint8_t>(percent));
        }
      } else if (param->read.handle == this->hours_handle_) {
        this->decode_hours_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->minutes_handle_) {
        this->decode_minutes_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->firmware_version_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, &VolcanoBleClientObserver::on_firmware_version,
                           "Firmware version");
      } else if (param->read.handle == this->ble_firmware_version_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, &VolcanoBleClientObserver::on_ble_firmware_version,
                           "Firmware BLE version");
      } else if (param->read.handle == this->serial_number_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, &VolcanoBleClientObserver::on_serial_number,
                           "Serial number");
      } else if (param->read.handle == this->power_supply_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, &VolcanoBleClientObserver::on_power_supply,
                           "Power supply rating");
      } else if (param->read.handle == this->product_line_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, &VolcanoBleClientObserver::on_product_line,
                           "Product line");
      }

      if (completes_static_read)
        this->issue_next_static_read_();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->status_handle_) {
        this->decode_status_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->countdown_handle_) {
        this->decode_countdown_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->current_temp_handle_) {
        this->decode_current_temperature_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->target_temp_handle_) {
        this->decode_target_temperature_(param->notify.value, param->notify.value_len, false);
      } else if (param->notify.handle == this->display_register_handle_) {
        this->decode_display_register_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->vibration_handle_) {
        this->decode_vibration_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->hours_handle_) {
        this->decode_hours_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->minutes_handle_) {
        this->decode_minutes_(param->notify.value, param->notify.value_len);
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.handle == this->duration_handle_) {
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Auto-shutoff duration write failed, status=%d", param->write.status);
          if (this->observer_ != nullptr)
            this->observer_->on_write_failed(VolcanoField::AUTO_SHUTOFF_DURATION);
          break;
        }
        if (this->observer_ != nullptr)
          this->observer_->on_write_acked(VolcanoField::AUTO_SHUTOFF_DURATION);
        this->read_auto_shutoff_duration_();
      } else if (param->write.handle == this->heater_on_handle_ || param->write.handle == this->heater_off_handle_) {
        // Per the trigger-characteristics note in docs/protocol/commands.md,
        // reading one of these back always returns 0x00 regardless of
        // effect, so it cannot confirm the action -- only the status/flags
        // register notification (STATE-008) does that.
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Trigger write to handle 0x%04x failed, status=%d", param->write.handle, param->write.status);
          if (this->observer_ != nullptr)
            this->observer_->on_write_failed(VolcanoField::HEATER);
        } else if (this->observer_ != nullptr) {
          this->observer_->on_write_acked(VolcanoField::HEATER);
        }
      } else if (param->write.handle == this->pump_on_handle_ || param->write.handle == this->pump_off_handle_) {
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Trigger write to handle 0x%04x failed, status=%d", param->write.handle, param->write.status);
          if (this->observer_ != nullptr)
            this->observer_->on_write_failed(VolcanoField::PUMP);
        } else if (this->observer_ != nullptr) {
          this->observer_->on_write_acked(VolcanoField::PUMP);
        }
      } else if (param->write.handle == this->display_register_handle_) {
        // Notifies, so no read-back: decode_display_register_() reports
        // via the observer on its own. write_display_on_cooling() and
        // write_display_units_fahrenheit() can each be called independently
        // of the other, so which field this completion belongs to comes
        // from the FIFO, not whichever call happened to run most recently
        // -- see display_register_write_queue.h.
        auto field = this->display_register_pending_writes_.pop();
        if (!field.has_value()) {
          ESP_LOGW(TAG, "Display/units register write completed with no pending write tracked");
        } else {
          if (param->write.status != ESP_GATT_OK) {
            ESP_LOGW(TAG, "Display/units register write failed, status=%d", param->write.status);
            if (this->observer_ != nullptr)
              this->observer_->on_write_failed(*field);
          } else if (this->observer_ != nullptr) {
            this->observer_->on_write_acked(*field);
          }
        }
      } else if (param->write.handle == this->vibration_handle_) {
        // No read-back here: unlike the write-only-readable settings, this
        // register notifies, so decode_vibration_() reports the resulting
        // state on its own.
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Vibration write failed, status=%d", param->write.status);
          if (this->observer_ != nullptr)
            this->observer_->on_write_failed(VolcanoField::VIBRATION);
        } else if (this->observer_ != nullptr) {
          this->observer_->on_write_acked(VolcanoField::VIBRATION);
        }
      } else if (param->write.handle == this->led_brightness_handle_) {
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "LED brightness write failed, status=%d", param->write.status);
          if (this->observer_ != nullptr)
            this->observer_->on_write_failed(VolcanoField::LED_BRIGHTNESS);
          break;
        }
        if (this->observer_ != nullptr)
          this->observer_->on_write_acked(VolcanoField::LED_BRIGHTNESS);
        auto status = esp_ble_gattc_read_char(this->client_->get_gattc_if(), this->client_->get_conn_id(),
                                              this->led_brightness_handle_, ESP_GATT_AUTH_REQ_NONE);
        if (status) {
          ESP_LOGW(TAG, "esp_ble_gattc_read_char(LED brightness) failed, status=%d", status);
        }
      } else if (param->write.handle == this->target_temp_handle_) {
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Target temperature write failed, status=%d", param->write.status);
          if (this->observer_ != nullptr)
            this->observer_->on_write_failed(VolcanoField::TARGET_TEMPERATURE);
          break;
        }
        if (this->observer_ != nullptr)
          this->observer_->on_write_acked(VolcanoField::TARGET_TEMPERATURE);
        // Read-back for the same reason as the auto-shutoff duration above.
        // Per STATE-013, this project's own writes are almost never echoed
        // by a notification, so an explicit read is what actually confirms
        // it here.
        auto status = esp_ble_gattc_read_char(this->client_->get_gattc_if(), this->client_->get_conn_id(),
                                              this->target_temp_handle_, ESP_GATT_AUTH_REQ_NONE);
        if (status) {
          ESP_LOGW(TAG, "esp_ble_gattc_read_char(target temperature) failed, status=%d", status);
        }
      }
      break;
    }
    default:
      break;
  }
}

void VolcanoBleClient::decode_status_(const uint8_t *value, uint16_t value_len) {
  // STATE-008: the attribute is 4 bytes with the upper two always observed
  // as zero, so only the low 16 bits, little-endian, are decoded.
  if (value_len < 2) {
    ESP_LOGW(TAG, "Status/flags register value too short (%u bytes)", value_len);
    return;
  }
  uint16_t status = encode_uint16(value[1], value[0]);
  bool heater_on = (status & STATUS_BIT_HEATER_ON) != 0;
  bool pump_on = (status & STATUS_BIT_PUMP_ON) != 0;
  ESP_LOGI(TAG, "Heater %s, pump %s (status=0x%04x)", heater_on ? "on" : "off", pump_on ? "on" : "off", status);
  // Reported from the register rather than from whatever was last
  // commanded: the device switches its own actuators off at auto-shutoff
  // expiry (STATE-011) and on a downward 40 degC crossing (STATE-012), with
  // no command involved, and reports panel-driven changes identically.
  if (this->observer_ != nullptr)
    this->observer_->on_actuator_state(heater_on, pump_on);
}

void VolcanoBleClient::decode_countdown_(const uint8_t *value, uint16_t value_len) {
  // STATE-005: 2-byte little-endian seconds.
  if (value_len < 2) {
    ESP_LOGW(TAG, "Auto-shutoff countdown value too short (%u bytes)", value_len);
    return;
  }
  uint16_t seconds = encode_uint16(value[1], value[0]);
  ESP_LOGI(TAG, "Auto-shutoff countdown: %u s", seconds);
  if (this->observer_ != nullptr)
    this->observer_->on_auto_shutoff_countdown(seconds);
}

// STATE-007/CMD-001: both temperature characteristics share a 4-byte
// little-endian encoding in units of 0.1 degC.
static bool decode_decidegrees_c(const uint8_t *value, uint16_t value_len, uint32_t *out_raw) {
  if (value_len < 4) {
    return false;
  }
  *out_raw = encode_uint32(value[3], value[2], value[1], value[0]);
  return true;
}

void VolcanoBleClient::decode_current_temperature_(const uint8_t *value, uint16_t value_len) {
  uint32_t raw;
  if (!decode_decidegrees_c(value, value_len, &raw)) {
    ESP_LOGW(TAG, "Current temperature value too short (%u bytes)", value_len);
    return;
  }
  // STATE-012: 0 means no reading available -- the device stops reporting
  // current temperature below 40 degC whenever the heater is off -- not a
  // true 0 degC reading, so it must never be logged, or reported, as a
  // temperature.
  if (raw == 0) {
    ESP_LOGI(TAG, "Current temperature: no reading (below 40 C, heater off)");
    if (this->observer_ != nullptr)
      this->observer_->on_current_temperature(false, 0.0f);
  } else {
    ESP_LOGI(TAG, "Current temperature: %.1f C", raw / 10.0f);
    if (this->observer_ != nullptr)
      this->observer_->on_current_temperature(true, raw / 10.0f);
  }
}

void VolcanoBleClient::decode_target_temperature_(const uint8_t *value, uint16_t value_len, bool from_read) {
  uint32_t raw;
  if (!decode_decidegrees_c(value, value_len, &raw)) {
    ESP_LOGW(TAG, "Target temperature value too short (%u bytes)", value_len);
    return;
  }
  ESP_LOGI(TAG, "Target temperature: %.1f C", raw / 10.0f);
  if (this->observer_ != nullptr)
    this->observer_->on_target_temperature(raw / 10.0f, from_read);
}

void VolcanoBleClient::decode_vibration_(const uint8_t *value, uint16_t value_len) {
  // CHAR-010: reads back as a 4-byte register value, not in the
  // mask-and-action form used to write it.
  if (value_len < 4) {
    ESP_LOGW(TAG, "Vibration register value too short (%u bytes)", value_len);
    return;
  }
  uint32_t reg = encode_uint32(value[3], value[2], value[1], value[0]);
  // Inverted: the bit being clear is what means enabled.
  bool enabled = (reg & VIBRATION_BIT_DISABLED) == 0;
  ESP_LOGI(TAG, "Vibration %s (register=0x%08x)", enabled ? "on" : "off", reg);
  if (this->observer_ != nullptr)
    this->observer_->on_vibration(enabled);
}

void VolcanoBleClient::decode_display_register_(const uint8_t *value, uint16_t value_len) {
  if (value_len < 4) {
    ESP_LOGW(TAG, "Display/units register value too short (%u bytes)", value_len);
    return;
  }
  uint32_t reg = encode_uint32(value[3], value[2], value[1], value[0]);
  // Inverted, as for vibration: the bit being clear is what means enabled.
  bool enabled = (reg & DISPLAY_ON_COOLING_BIT_DISABLED) == 0;
  bool fahrenheit = (reg & DISPLAY_UNITS_BIT_FAHRENHEIT) != 0;

  // This register notifies on every 1 degC change of current temperature
  // (STATE-009), so it arrives every few seconds throughout a run while
  // this setting has not moved at all. Log only actual changes to
  // display-on-cooling; reporting an unchanged state to the observer is
  // harmless (VolcanoDevice dedups it), but a log line every few seconds
  // is not.
  int8_t state = enabled ? 1 : 0;
  if (state != this->display_on_cooling_log_state_) {
    ESP_LOGI(TAG, "Display on cooling %s (register=0x%08x)", enabled ? "on" : "off", reg);
    this->display_on_cooling_log_state_ = state;
  }
  if (this->observer_ != nullptr)
    this->observer_->on_display_register(enabled, fahrenheit);
}

void VolcanoBleClient::decode_hours_(const uint8_t *value, uint16_t value_len) {
  // STATE-001: 4-byte little-endian count of heater-on hours, carried into
  // from the minutes counter wrapping rather than from wall-clock time.
  if (value_len < 4) {
    ESP_LOGW(TAG, "Hours of operation value too short (%u bytes)", value_len);
    return;
  }
  uint32_t hours = encode_uint32(value[3], value[2], value[1], value[0]);
  ESP_LOGI(TAG, "Hours of operation: %u", hours);
  if (this->observer_ != nullptr)
    this->observer_->on_hours_of_operation(hours);
}

void VolcanoBleClient::decode_minutes_(const uint8_t *value, uint16_t value_len) {
  // STATE-006: 2-byte little-endian, 0-59, the minutes component of the
  // heater-runtime meter paired with CHAR-022. It advances only while the
  // heater is on, and carries its sub-minute remainder across off periods.
  if (value_len < 2) {
    ESP_LOGW(TAG, "Minutes of operation value too short (%u bytes)", value_len);
    return;
  }
  uint16_t minutes = encode_uint16(value[1], value[0]);
  ESP_LOGI(TAG, "Minutes of operation: %u", minutes);
  if (this->observer_ != nullptr)
    this->observer_->on_minutes_of_operation(minutes);
}

void VolcanoBleClient::queue_static_reads_() {
  // Order is deliberate: the auto-shutoff duration first, since it is the
  // only one of these that affects how the device behaves, and the device
  // information after, none of which anything waits on.
  const uint16_t handles[] = {
      this->duration_handle_,         this->led_brightness_handle_,
      this->firmware_version_handle_, this->ble_firmware_version_handle_,
      this->serial_number_handle_,    this->power_supply_handle_,
      this->product_line_handle_,
  };
  // Compile-time link between this list and the queue's capacity: a future
  // addition here that isn't matched by a StaticReadQueue::CAPACITY bump
  // would otherwise fail silently (enqueue() just drops the overflow) rather
  // than at build time.
  static_assert(sizeof(handles) / sizeof(handles[0]) <= StaticReadQueue::CAPACITY,
                "StaticReadQueue::CAPACITY is too small for the handles listed above");
  this->static_reads_.reset();
  for (uint16_t handle : handles)
    this->static_reads_.enqueue(handle);
  this->issue_next_static_read_();
}

void VolcanoBleClient::issue_next_static_read_() {
  // Each read is issued from the previous one's completion, so only one is
  // ever outstanding. A read that cannot even be issued is skipped rather
  // than retried, so one unreadable characteristic cannot strand the rest.
  while (!this->static_reads_.done()) {
    uint16_t handle = this->static_reads_.current();
    auto status = esp_ble_gattc_read_char(this->client_->get_gattc_if(), this->client_->get_conn_id(), handle,
                                          ESP_GATT_AUTH_REQ_NONE);
    if (status == ESP_OK)
      return;
    ESP_LOGW(TAG, "esp_ble_gattc_read_char(0x%04x) failed, status=%d", handle, status);
    this->static_reads_.skip();
  }
  // The static read sweep has completed -- ADR-0009: this, not merely the
  // link being up, is what "ready" means, since the state model is not
  // populated until this point. static_sweep_done_ gates
  // write_auto_shutoff_duration()/write_led_brightness() so their read-backs
  // can never again collide with this sweep's own reads on the same handle
  // -- see the member's declaration in volcano_ble_client.h.
  this->static_sweep_done_ = true;
  if (this->observer_ != nullptr)
    this->observer_->on_ready();
}

void VolcanoBleClient::decode_text_(const uint8_t *value, uint16_t value_len,
                                    void (VolcanoBleClientObserver::*report)(const std::string &), const char *name) {
  // Fixed-width ASCII fields, padded to their full width -- with spaces on
  // some characteristics and zero characters on others -- so both forms of
  // padding are trimmed rather than reported as part of the value.
  std::string text;
  text.reserve(value_len);
  for (uint16_t i = 0; i < value_len; i++) {
    if (value[i] == '\0')
      break;
    text.push_back(static_cast<char>(value[i]));
  }
  while (!text.empty() && text.back() == ' ')
    text.pop_back();

  ESP_LOGI(TAG, "%s: %s", name, text.c_str());
  if (this->observer_ != nullptr)
    (this->observer_->*report)(text);
}

void VolcanoBleClient::read_auto_shutoff_duration_() {
  if (this->duration_handle_ == 0)
    return;
  auto status = esp_ble_gattc_read_char(this->client_->get_gattc_if(), this->client_->get_conn_id(),
                                        this->duration_handle_, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_read_char(duration) failed, status=%d", status);
  }
}

bool VolcanoBleClient::write_auto_shutoff_duration(uint16_t seconds) {
  if (seconds < MIN_AUTO_SHUTOFF_DURATION_SECONDS || seconds > MAX_AUTO_SHUTOFF_DURATION_SECONDS) {
    ESP_LOGW(TAG,
             "Refusing to set auto-shutoff duration to %u s: outside the %u-%u s range confirmed accepted (CMD-003)",
             seconds, MIN_AUTO_SHUTOFF_DURATION_SECONDS, MAX_AUTO_SHUTOFF_DURATION_SECONDS);
    return false;
  }
  if (this->duration_handle_ == 0) {
    ESP_LOGW(TAG, "Auto-shutoff duration characteristic not resolved; not connected?");
    return false;
  }
  if (!this->static_sweep_done_) {
    // See static_sweep_done_'s declaration: this write's own read-back
    // would otherwise race the static-read sweep's own read of this same
    // handle, since ESP_GATTC_READ_CHAR_EVT can't tell the two apart.
    ESP_LOGW(TAG, "Refusing to set auto-shutoff duration: still reading initial device state, try again shortly");
    return false;
  }
  // CMD-003's confirmed encoding: 2-byte little-endian seconds.
  uint8_t payload[2] = {static_cast<uint8_t>(seconds & 0xFF), static_cast<uint8_t>((seconds >> 8) & 0xFF)};
  ESP_LOGI(TAG, "Setting auto-shutoff duration to %u s", seconds);
  auto status =
      esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(), this->duration_handle_,
                               sizeof(payload), payload, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  return true;
}

bool VolcanoBleClient::write_led_brightness(uint8_t percent) {
  if (percent > MAX_LED_BRIGHTNESS_PERCENT) {
    ESP_LOGW(TAG, "Refusing to set LED brightness to %u%%: outside the 0-%u scale CMD-002 records", percent,
             MAX_LED_BRIGHTNESS_PERCENT);
    return false;
  }
  if (this->led_brightness_handle_ == 0) {
    ESP_LOGW(TAG, "LED brightness characteristic not resolved; not connected?");
    return false;
  }
  if (!this->static_sweep_done_) {
    // See static_sweep_done_'s declaration: this write's own read-back
    // would otherwise race the static-read sweep's own read of this same
    // handle, since ESP_GATTC_READ_CHAR_EVT can't tell the two apart.
    ESP_LOGW(TAG, "Refusing to set LED brightness: still reading initial device state, try again shortly");
    return false;
  }
  // CMD-002's confirmed encoding: 2 bytes, only the low one significant.
  uint8_t payload[2] = {percent, 0};
  ESP_LOGI(TAG, "Setting LED brightness to %u%%", percent);
  auto status = esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(),
                                         this->led_brightness_handle_, sizeof(payload), payload,
                                         ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  return true;
}

bool VolcanoBleClient::write_vibration(bool enabled) {
  if (this->vibration_handle_ == 0) {
    ESP_LOGW(TAG, "Vibration characteristic not resolved; not connected?");
    return false;
  }
  // CMD-004's confirmed mask-and-action form: a 2-byte little-endian bit
  // mask, then 00 to clear that bit or 01 to set it, then a padding byte.
  // The write names only the bit being changed, so the register's other
  // bits -- several of which are unidentified -- are neither read first nor
  // disturbed.
  //
  // Clear enables, per VIBRATION_BIT_DISABLED above.
  uint8_t payload[4] = {
      static_cast<uint8_t>(VIBRATION_BIT_DISABLED & 0xFF),
      static_cast<uint8_t>((VIBRATION_BIT_DISABLED >> 8) & 0xFF),
      static_cast<uint8_t>(enabled ? 0x00 : 0x01),
      0x00,
  };
  ESP_LOGI(TAG, "Turning vibration %s", enabled ? "on" : "off");
  auto status =
      esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(), this->vibration_handle_,
                               sizeof(payload), payload, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  return true;
}

bool VolcanoBleClient::write_display_units_fahrenheit(bool fahrenheit) {
  if (this->display_register_handle_ == 0) {
    ESP_LOGW(TAG, "Display/units register not resolved; not connected?");
    return false;
  }
  // CMD-010's confirmed mask-and-action form, naming only the units bit.
  // The action byte is NOT inverted here: set selects Fahrenheit, unlike
  // the two settings below where clear enables.
  uint8_t payload[4] = {
      static_cast<uint8_t>(DISPLAY_UNITS_BIT_FAHRENHEIT & 0xFF),
      static_cast<uint8_t>((DISPLAY_UNITS_BIT_FAHRENHEIT >> 8) & 0xFF),
      static_cast<uint8_t>(fahrenheit ? 0x01 : 0x00),
      0x00,
  };
  ESP_LOGI(TAG, "Setting display units to %s", fahrenheit ? "Fahrenheit" : "Celsius");
  auto status = esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(),
                                         this->display_register_handle_, sizeof(payload), payload,
                                         ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  // Queued only once the write is actually in flight, so a synchronous
  // esp_ble_gattc_write_char() failure above never leaves a stale entry for
  // an ESP_GATTC_WRITE_CHAR_EVT that will now never arrive.
  this->display_register_pending_writes_.push(VolcanoField::DISPLAY_UNITS_FAHRENHEIT);
  return true;
}

bool VolcanoBleClient::write_display_on_cooling(bool enabled) {
  if (this->display_register_handle_ == 0) {
    ESP_LOGW(TAG, "Display/units register not resolved; not connected?");
    return false;
  }
  // CMD-005's confirmed mask-and-action form, identical in shape to CMD-004
  // and naming only the display-on-cooling bit -- so the units bit and the
  // register's unidentified bits are left alone.
  uint8_t payload[4] = {
      static_cast<uint8_t>(DISPLAY_ON_COOLING_BIT_DISABLED & 0xFF),
      static_cast<uint8_t>((DISPLAY_ON_COOLING_BIT_DISABLED >> 8) & 0xFF),
      static_cast<uint8_t>(enabled ? 0x00 : 0x01),
      0x00,
  };
  ESP_LOGI(TAG, "Turning display on cooling %s", enabled ? "on" : "off");
  auto status = esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(),
                                         this->display_register_handle_, sizeof(payload), payload,
                                         ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  this->display_register_pending_writes_.push(VolcanoField::DISPLAY_ON_COOLING);
  return true;
}

// CMD-006 through CMD-009: every trigger characteristic accepts only the
// single value 0x00 -- the only value ever observed written to any of them
// (see the trigger-characteristics note in docs/protocol/commands.md) -- so
// this writes nothing else.
bool VolcanoBleClient::write_trigger_(uint16_t handle, const char *name) {
  if (handle == 0) {
    ESP_LOGW(TAG, "%s trigger characteristic not resolved; not connected?", name);
    return false;
  }
  uint8_t payload = 0x00;
  ESP_LOGI(TAG, "%s", name);
  auto status = esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(), handle,
                                         sizeof(payload), &payload, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char(%s) failed, status=%d", name, status);
    return false;
  }
  return true;
}

bool VolcanoBleClient::write_heater(bool on) {
  return this->write_trigger_(on ? this->heater_on_handle_ : this->heater_off_handle_,
                              on ? "Turning heater on" : "Turning heater off");
}

bool VolcanoBleClient::write_pump(bool on) {
  return this->write_trigger_(on ? this->pump_on_handle_ : this->pump_off_handle_,
                              on ? "Turning pump on" : "Turning pump off");
}

bool VolcanoBleClient::write_target_temperature(float celsius) {
  long decidegrees_signed = std::lroundf(celsius * 10.0f);
  if (decidegrees_signed < MIN_TARGET_TEMPERATURE_DECIDEGREES ||
      decidegrees_signed > MAX_TARGET_TEMPERATURE_DECIDEGREES) {
    ESP_LOGW(TAG,
             "Refusing to set target temperature to %.1f C: outside the %.1f-%.1f C range confirmed accepted "
             "(CMD-001)",
             celsius, MIN_TARGET_TEMPERATURE_DECIDEGREES / 10.0f, MAX_TARGET_TEMPERATURE_DECIDEGREES / 10.0f);
    return false;
  }
  if (this->target_temp_handle_ == 0) {
    ESP_LOGW(TAG, "Target temperature characteristic not resolved; not connected?");
    return false;
  }
  uint16_t decidegrees = static_cast<uint16_t>(decidegrees_signed);
  // CMD-001's confirmed encoding: 4-byte little-endian deci-degrees Celsius,
  // matching the read-side encoding decode_decidegrees_c() above decodes.
  uint8_t payload[4] = {
      static_cast<uint8_t>(decidegrees & 0xFF),
      static_cast<uint8_t>((decidegrees >> 8) & 0xFF),
      0,
      0,
  };
  ESP_LOGI(TAG, "Setting target temperature to %.1f C", decidegrees / 10.0f);
  auto status =
      esp_ble_gattc_write_char(this->client_->get_gattc_if(), this->client_->get_conn_id(), this->target_temp_handle_,
                               sizeof(payload), payload, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  return true;
}

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
