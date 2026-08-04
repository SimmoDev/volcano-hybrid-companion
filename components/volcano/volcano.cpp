#include "volcano.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace volcano {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const TAG = "volcano";

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
// it is untested. set_auto_shutoff_duration_seconds() refuses both rather
// than writing an untested value (ADR-0005).
static const uint16_t MIN_AUTO_SHUTOFF_DURATION_SECONDS = 60;
static const uint16_t MAX_AUTO_SHUTOFF_DURATION_SECONDS = 21600;

// CMD-001: the official app's UI spans 40.0-230.0 degC (400-2300 in this
// characteristic's deci-degrees-Celsius encoding); that is the only range
// Confirmed accepted. What the device does with a value outside it is
// Unknown, so set_target_temperature_decidegrees() refuses them rather than
// writing an untested value (ADR-0005).
static const uint16_t MIN_TARGET_TEMPERATURE_DECIDEGREES = 400;
static const uint16_t MAX_TARGET_TEMPERATURE_DECIDEGREES = 2300;

static void subscribe(esphome::ble_client::BLEClient *client, uint16_t handle, const char *name) {
  auto status = esp_ble_gattc_register_for_notify(client->get_gattc_if(), client->get_remote_bda(), handle);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify(%s) failed, status=%d", name, status);
  }
}

void VolcanoComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Volcano component...");
  // Purely event-driven: connection state, characteristic resolution and
  // reads all happen from BLE callbacks below, so the ESPHome loop is not
  // needed.
  this->disable_loop();
}

void VolcanoComponent::loop() {}

void VolcanoComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Volcano:");
  ESP_LOGCONFIG(TAG, "  Read-only: status/flags register (CHAR-008), auto-shutoff countdown (CHAR-016),");
  ESP_LOGCONFIG(TAG, "    current temperature (CHAR-013), hours/minutes of operation (CHAR-022/023).");
  ESP_LOGCONFIG(TAG, "  Read once per connection: firmware version (CHAR-005), firmware BLE version");
  ESP_LOGCONFIG(TAG, "    (CHAR-006), serial number (CHAR-007), power supply (CHAR-024),");
  ESP_LOGCONFIG(TAG, "    product line (CHAR-025).");
  ESP_LOGCONFIG(TAG, "  Write: auto-shutoff duration (CHAR-017), %u-%u s;", MIN_AUTO_SHUTOFF_DURATION_SECONDS,
                MAX_AUTO_SHUTOFF_DURATION_SECONDS);
  ESP_LOGCONFIG(TAG, "    heater on/off (CHAR-018/019), pump on/off (CHAR-020/021);");
  ESP_LOGCONFIG(TAG, "    target temperature (CHAR-014), %.1f-%.1f C.", MIN_TARGET_TEMPERATURE_DECIDEGREES / 10.0f,
                MAX_TARGET_TEMPERATURE_DECIDEGREES / 10.0f);
}

void VolcanoComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
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
      this->pending_subscriptions_ = 0;
      this->static_read_count_ = 0;
      this->static_read_index_ = 0;
      ESP_LOGI(TAG, "Disconnected; heater/pump/countdown/temperature state unknown");
      // Any configured entity keeps its last published value here rather
      // than going unavailable -- there is no ESPHome API this component
      // can use to mark one unavailable from custom C++ code.
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service discovery failed, status=%d", param->search_cmpl.status);
        break;
      }
      auto settings_service = espbt::ESPBTUUID::from_raw(SETTINGS_SERVICE_UUID);
      auto control_service = espbt::ESPBTUUID::from_raw(CONTROL_SERVICE_UUID);

      this->pending_subscriptions_ = 0;

      auto *status_chr = this->parent()->get_characteristic(settings_service,
                                                             espbt::ESPBTUUID::from_raw(STATUS_CHARACTERISTIC_UUID));
      if (status_chr == nullptr) {
        ESP_LOGW(TAG, "Status/flags register (CHAR-008) not found on device");
      } else {
        this->status_handle_ = status_chr->handle;
        // CONN-002: nothing is pushed on subscribing, so the initial value
        // is read explicitly once subscription completes -- see the
        // ESP_GATTC_REG_FOR_NOTIFY_EVT case below.
        this->pending_subscriptions_++;
        subscribe(this->parent(), this->status_handle_, "status/flags register");
      }

      auto *countdown_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(COUNTDOWN_CHARACTERISTIC_UUID));
      if (countdown_chr == nullptr) {
        ESP_LOGW(TAG, "Auto-shutoff countdown (CHAR-016) not found on device");
      } else {
        this->countdown_handle_ = countdown_chr->handle;
        this->pending_subscriptions_++;
        subscribe(this->parent(), this->countdown_handle_, "auto-shutoff countdown");
      }

      auto *duration_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(DURATION_CHARACTERISTIC_UUID));
      if (duration_chr == nullptr) {
        ESP_LOGW(TAG, "Auto-shutoff duration (CHAR-017) not found on device");
      } else {
        // Read/Write only, no Notify -- resolved here for
        // set_auto_shutoff_duration_seconds() below, not subscribed, and so
        // not counted in pending_subscriptions_.
        this->duration_handle_ = duration_chr->handle;
      }

      auto *current_temp_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(CURRENT_TEMP_CHARACTERISTIC_UUID));
      if (current_temp_chr == nullptr) {
        ESP_LOGW(TAG, "Current temperature (CHAR-013) not found on device");
      } else {
        this->current_temp_handle_ = current_temp_chr->handle;
        this->pending_subscriptions_++;
        subscribe(this->parent(), this->current_temp_handle_, "current temperature");
      }

      auto *target_temp_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(TARGET_TEMP_CHARACTERISTIC_UUID));
      if (target_temp_chr == nullptr) {
        ESP_LOGW(TAG, "Target temperature (CHAR-014) not found on device");
      } else {
        this->target_temp_handle_ = target_temp_chr->handle;
        this->pending_subscriptions_++;
        subscribe(this->parent(), this->target_temp_handle_, "target temperature");
      }

      auto *hours_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(HOURS_CHARACTERISTIC_UUID));
      if (hours_chr == nullptr) {
        ESP_LOGW(TAG, "Hours of operation (CHAR-022) not found on device");
      } else {
        this->hours_handle_ = hours_chr->handle;
        this->pending_subscriptions_++;
        subscribe(this->parent(), this->hours_handle_, "hours of operation");
      }

      auto *minutes_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(MINUTES_CHARACTERISTIC_UUID));
      if (minutes_chr == nullptr) {
        ESP_LOGW(TAG, "Minutes of operation (CHAR-023) not found on device");
      } else {
        this->minutes_handle_ = minutes_chr->handle;
        this->pending_subscriptions_++;
        subscribe(this->parent(), this->minutes_handle_, "minutes of operation");
      }

      // CHAR-018 through CHAR-021: one-byte Read/Write trigger
      // characteristics (CMD-006 through CMD-009), not Notify-capable, so
      // resolved here like the duration characteristic above and not
      // counted in pending_subscriptions_.
      auto *heater_on_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(HEATER_ON_CHARACTERISTIC_UUID));
      if (heater_on_chr == nullptr) {
        ESP_LOGW(TAG, "Heater on trigger (CHAR-018) not found on device");
      } else {
        this->heater_on_handle_ = heater_on_chr->handle;
      }

      auto *heater_off_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(HEATER_OFF_CHARACTERISTIC_UUID));
      if (heater_off_chr == nullptr) {
        ESP_LOGW(TAG, "Heater off trigger (CHAR-019) not found on device");
      } else {
        this->heater_off_handle_ = heater_off_chr->handle;
      }

      auto *pump_on_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(PUMP_ON_CHARACTERISTIC_UUID));
      if (pump_on_chr == nullptr) {
        ESP_LOGW(TAG, "Pump on trigger (CHAR-020) not found on device");
      } else {
        this->pump_on_handle_ = pump_on_chr->handle;
      }

      auto *pump_off_chr = this->parent()->get_characteristic(
          control_service, espbt::ESPBTUUID::from_raw(PUMP_OFF_CHARACTERISTIC_UUID));
      if (pump_off_chr == nullptr) {
        ESP_LOGW(TAG, "Pump off trigger (CHAR-021) not found on device");
      } else {
        this->pump_off_handle_ = pump_off_chr->handle;
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
        auto *chr =
            this->parent()->get_characteristic(settings_service, espbt::ESPBTUUID::from_raw(entry.uuid));
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
        this->node_state = espbt::ClientState::ESTABLISHED;
        this->queue_static_reads_();
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      uint16_t handle = param->reg_for_notify.handle;
      if (handle != this->status_handle_ && handle != this->countdown_handle_ &&
          handle != this->current_temp_handle_ && handle != this->target_temp_handle_ &&
          handle != this->hours_handle_ && handle != this->minutes_handle_)
        break;

      // node_state must not become ESTABLISHED until every subscription
      // issued in ESP_GATTC_SEARCH_CMPL_EVT above has been accounted for --
      // see the pending_subscriptions_ comment in volcano.h for why an
      // early ESTABLISHED here is a use-after-free, not just a race.
      if (this->pending_subscriptions_ > 0)
        this->pending_subscriptions_--;
      if (this->pending_subscriptions_ == 0) {
        this->node_state = espbt::ClientState::ESTABLISHED;
        // The characteristics with no notify are read here rather than
        // subscribed -- once every subscription has settled, so this
        // does not contend with the reads they each issue below.
        this->queue_static_reads_();
      }

      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Subscribing to handle 0x%04x failed, status=%d", handle, param->reg_for_notify.status);
        break;
      }
      auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), handle,
                                            ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_read_char(0x%04x) failed, status=%d", handle, status);
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      // Whether this completes the static read currently in flight. Noted
      // before anything else so a failed read still advances the queue
      // rather than stalling every read behind it. A duration read-back
      // after a write does not match, since the queue has finished by then.
      bool completes_static_read = this->static_read_index_ < this->static_read_count_ &&
                                   param->read.handle == this->static_reads_[this->static_read_index_];
      if (completes_static_read)
        this->static_read_index_++;

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
        // Read-back after set_auto_shutoff_duration_seconds()'s write, to
        // confirm the raw value stuck at the protocol level -- separate
        // from whether it is honoured at the next arming, per STATE-005
        // (ADR-0002 requires confirming device state from the device
        // itself rather than trusting the ATT write response alone).
        if (param->read.value_len < 2) {
          ESP_LOGW(TAG, "Auto-shutoff duration value too short (%u bytes)", param->read.value_len);
        } else {
          uint16_t seconds = encode_uint16(param->read.value[1], param->read.value[0]);
          ESP_LOGI(TAG, "Auto-shutoff duration: %u s", seconds);
          // Published in minutes, the unit the number entity carries.
          if (this->auto_shutoff_duration_number_ != nullptr)
            this->auto_shutoff_duration_number_->publish_state(seconds / 60.0f);
        }
      } else if (param->read.handle == this->current_temp_handle_) {
        this->decode_current_temperature_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->target_temp_handle_) {
        this->decode_target_temperature_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->hours_handle_) {
        this->decode_hours_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->minutes_handle_) {
        this->decode_minutes_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->firmware_version_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, this->firmware_version_text_sensor_,
                           "Firmware version");
      } else if (param->read.handle == this->ble_firmware_version_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, this->ble_firmware_version_text_sensor_,
                           "Firmware BLE version");
      } else if (param->read.handle == this->serial_number_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, this->serial_number_text_sensor_,
                           "Serial number");
      } else if (param->read.handle == this->power_supply_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, this->power_supply_text_sensor_,
                           "Power supply rating");
      } else if (param->read.handle == this->product_line_handle_) {
        this->decode_text_(param->read.value, param->read.value_len, this->product_line_text_sensor_, "Product line");
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
        this->decode_target_temperature_(param->notify.value, param->notify.value_len);
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
          break;
        }
        this->read_auto_shutoff_duration_();
      } else if (param->write.handle == this->heater_on_handle_ || param->write.handle == this->heater_off_handle_ ||
                 param->write.handle == this->pump_on_handle_ || param->write.handle == this->pump_off_handle_) {
        // Per the trigger-characteristics note in docs/protocol/commands.md,
        // reading one of these back always returns 0x00 regardless of
        // effect, so it cannot confirm the action -- only the status/flags
        // register notification (STATE-008) does that.
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Trigger write to handle 0x%04x failed, status=%d", param->write.handle,
                   param->write.status);
        }
      } else if (param->write.handle == this->target_temp_handle_) {
        if (param->write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Target temperature write failed, status=%d", param->write.status);
          break;
        }
        // Read-back for the same reason as the auto-shutoff duration above.
        // Per STATE-013, this project's own writes are almost never echoed
        // by a notification, so an explicit read is what actually confirms
        // it here.
        auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
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

void VolcanoComponent::decode_status_(const uint8_t *value, uint16_t value_len) {
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
  // Published from the register rather than from whatever was last
  // commanded: the device switches its own actuators off at auto-shutoff
  // expiry (STATE-011) and on a downward 40 degC crossing (STATE-012), with
  // no command involved, and reports panel-driven changes identically.
  if (this->heater_switch_ != nullptr)
    this->heater_switch_->publish_state(heater_on);
  if (this->pump_switch_ != nullptr)
    this->pump_switch_->publish_state(pump_on);
}

void VolcanoComponent::decode_countdown_(const uint8_t *value, uint16_t value_len) {
  // STATE-005: 2-byte little-endian seconds.
  if (value_len < 2) {
    ESP_LOGW(TAG, "Auto-shutoff countdown value too short (%u bytes)", value_len);
    return;
  }
  uint16_t seconds = encode_uint16(value[1], value[0]);
  ESP_LOGI(TAG, "Auto-shutoff countdown: %u s", seconds);
  if (this->auto_shutoff_countdown_sensor_ != nullptr)
    this->auto_shutoff_countdown_sensor_->publish_state(seconds);
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

void VolcanoComponent::decode_current_temperature_(const uint8_t *value, uint16_t value_len) {
  uint32_t raw;
  if (!decode_decidegrees_c(value, value_len, &raw)) {
    ESP_LOGW(TAG, "Current temperature value too short (%u bytes)", value_len);
    return;
  }
  // STATE-012: 0 means no reading available -- the device stops reporting
  // current temperature below 40 degC whenever the heater is off -- not a
  // true 0 degC reading, so it must never be logged, or published, as a
  // temperature. NAN is the ESPHome convention for "no valid reading".
  if (raw == 0) {
    ESP_LOGI(TAG, "Current temperature: no reading (below 40 C, heater off)");
    if (this->current_temperature_sensor_ != nullptr)
      this->current_temperature_sensor_->publish_state(NAN);
  } else {
    ESP_LOGI(TAG, "Current temperature: %.1f C", raw / 10.0f);
    if (this->current_temperature_sensor_ != nullptr)
      this->current_temperature_sensor_->publish_state(raw / 10.0f);
  }
}

void VolcanoComponent::decode_target_temperature_(const uint8_t *value, uint16_t value_len) {
  uint32_t raw;
  if (!decode_decidegrees_c(value, value_len, &raw)) {
    ESP_LOGW(TAG, "Target temperature value too short (%u bytes)", value_len);
    return;
  }
  ESP_LOGI(TAG, "Target temperature: %.1f C", raw / 10.0f);
  if (this->target_temperature_number_ != nullptr)
    this->target_temperature_number_->publish_state(raw / 10.0f);
}

void VolcanoComponent::decode_hours_(const uint8_t *value, uint16_t value_len) {
  // STATE-001: 4-byte little-endian count of heater-on hours, carried into
  // from the minutes counter wrapping rather than from wall-clock time.
  if (value_len < 4) {
    ESP_LOGW(TAG, "Hours of operation value too short (%u bytes)", value_len);
    return;
  }
  uint32_t hours = encode_uint32(value[3], value[2], value[1], value[0]);
  ESP_LOGI(TAG, "Hours of operation: %u", hours);
  if (this->hours_sensor_ != nullptr)
    this->hours_sensor_->publish_state(hours);
}

void VolcanoComponent::decode_minutes_(const uint8_t *value, uint16_t value_len) {
  // STATE-006: 2-byte little-endian, 0-59, the minutes component of the
  // heater-runtime meter paired with CHAR-022. It advances only while the
  // heater is on, and carries its sub-minute remainder across off periods.
  if (value_len < 2) {
    ESP_LOGW(TAG, "Minutes of operation value too short (%u bytes)", value_len);
    return;
  }
  uint16_t minutes = encode_uint16(value[1], value[0]);
  ESP_LOGI(TAG, "Minutes of operation: %u", minutes);
  if (this->minutes_sensor_ != nullptr)
    this->minutes_sensor_->publish_state(minutes);
}

void VolcanoComponent::queue_static_reads_() {
  // Order is deliberate: the auto-shutoff duration first, since it is the
  // only one of these that affects how the device behaves, and the device
  // information after, none of which anything waits on.
  const uint16_t handles[] = {
      this->duration_handle_,     this->firmware_version_handle_, this->ble_firmware_version_handle_,
      this->serial_number_handle_, this->power_supply_handle_,    this->product_line_handle_,
  };
  this->static_read_count_ = 0;
  this->static_read_index_ = 0;
  for (uint16_t handle : handles) {
    if (handle != 0 && this->static_read_count_ < MAX_STATIC_READS)
      this->static_reads_[this->static_read_count_++] = handle;
  }
  this->issue_next_static_read_();
}

void VolcanoComponent::issue_next_static_read_() {
  // Each read is issued from the previous one's completion, so only one is
  // ever outstanding. A read that cannot even be issued is skipped rather
  // than retried, so one unreadable characteristic cannot strand the rest.
  while (this->static_read_index_ < this->static_read_count_) {
    uint16_t handle = this->static_reads_[this->static_read_index_];
    auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), handle,
                                          ESP_GATT_AUTH_REQ_NONE);
    if (status == ESP_OK)
      return;
    ESP_LOGW(TAG, "esp_ble_gattc_read_char(0x%04x) failed, status=%d", handle, status);
    this->static_read_index_++;
  }
}

void VolcanoComponent::decode_text_(const uint8_t *value, uint16_t value_len, text_sensor::TextSensor *sensor,
                                    const char *name) {
  // Fixed-width ASCII fields, padded to their full width -- with spaces on
  // some characteristics and zero characters on others -- so both forms of
  // padding are trimmed rather than published as part of the value.
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
  if (sensor != nullptr)
    sensor->publish_state(text);
}

void VolcanoComponent::read_auto_shutoff_duration_() {
  if (this->duration_handle_ == 0)
    return;
  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                        this->duration_handle_, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_read_char(duration) failed, status=%d", status);
  }
}

void VolcanoComponent::set_auto_shutoff_duration_seconds(uint16_t seconds) {
  if (seconds < MIN_AUTO_SHUTOFF_DURATION_SECONDS || seconds > MAX_AUTO_SHUTOFF_DURATION_SECONDS) {
    ESP_LOGW(TAG,
             "Refusing to set auto-shutoff duration to %u s: outside the %u-%u s range confirmed accepted (CMD-003)",
             seconds, MIN_AUTO_SHUTOFF_DURATION_SECONDS, MAX_AUTO_SHUTOFF_DURATION_SECONDS);
    return;
  }
  if (this->duration_handle_ == 0) {
    ESP_LOGW(TAG, "Auto-shutoff duration characteristic not resolved; not connected?");
    return;
  }
  // CMD-003's confirmed encoding: 2-byte little-endian seconds.
  uint8_t payload[2] = {static_cast<uint8_t>(seconds & 0xFF), static_cast<uint8_t>((seconds >> 8) & 0xFF)};
  ESP_LOGI(TAG, "Setting auto-shutoff duration to %u s", seconds);
  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                         this->duration_handle_, sizeof(payload), payload, ESP_GATT_WRITE_TYPE_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
  }
}

// CMD-006 through CMD-009: every trigger characteristic accepts only the
// single value 0x00 -- the only value ever observed written to any of them
// (see the trigger-characteristics note in docs/protocol/commands.md) -- so
// this writes nothing else.
static void write_trigger(esphome::ble_client::BLEClient *client, uint16_t handle, const char *name) {
  if (handle == 0) {
    ESP_LOGW(TAG, "%s trigger characteristic not resolved; not connected?", name);
    return;
  }
  uint8_t payload = 0x00;
  ESP_LOGI(TAG, "%s", name);
  auto status = esp_ble_gattc_write_char(client->get_gattc_if(), client->get_conn_id(), handle, sizeof(payload),
                                         &payload, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char(%s) failed, status=%d", name, status);
  }
}

void VolcanoComponent::turn_heater_on() { write_trigger(this->parent(), this->heater_on_handle_, "Turning heater on"); }

void VolcanoComponent::turn_heater_off() {
  write_trigger(this->parent(), this->heater_off_handle_, "Turning heater off");
}

void VolcanoComponent::turn_pump_on() { write_trigger(this->parent(), this->pump_on_handle_, "Turning pump on"); }

void VolcanoComponent::turn_pump_off() { write_trigger(this->parent(), this->pump_off_handle_, "Turning pump off"); }

void VolcanoComponent::set_target_temperature_decidegrees(uint16_t decidegrees) {
  if (decidegrees < MIN_TARGET_TEMPERATURE_DECIDEGREES || decidegrees > MAX_TARGET_TEMPERATURE_DECIDEGREES) {
    ESP_LOGW(TAG,
             "Refusing to set target temperature to %.1f C: outside the %.1f-%.1f C range confirmed accepted "
             "(CMD-001)",
             decidegrees / 10.0f, MIN_TARGET_TEMPERATURE_DECIDEGREES / 10.0f,
             MAX_TARGET_TEMPERATURE_DECIDEGREES / 10.0f);
    return;
  }
  if (this->target_temp_handle_ == 0) {
    ESP_LOGW(TAG, "Target temperature characteristic not resolved; not connected?");
    return;
  }
  // CMD-001's confirmed encoding: 4-byte little-endian deci-degrees Celsius,
  // matching the read-side encoding decode_decidegrees_c() above decodes.
  uint8_t payload[4] = {
      static_cast<uint8_t>(decidegrees & 0xFF),
      static_cast<uint8_t>((decidegrees >> 8) & 0xFF),
      0,
      0,
  };
  ESP_LOGI(TAG, "Setting target temperature to %.1f C", decidegrees / 10.0f);
  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                         this->target_temp_handle_, sizeof(payload), payload, ESP_GATT_WRITE_TYPE_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
  }
}

// The entity's own min/max have already clamped `value` by the time this is
// reached, but they are configurable and so cannot be relied on as the
// safety bound -- set_target_temperature_decidegrees() re-checks against the
// range CMD-001 confirms accepted. The guard here is only against the
// conversion overflowing the 16-bit encoding, which a wide enough configured
// max would otherwise do silently.
void VolcanoTargetTemperatureNumber::control(float value) {
  long decidegrees = lroundf(value * 10.0f);
  if (decidegrees < 0 || decidegrees > UINT16_MAX) {
    ESP_LOGW(TAG, "Ignoring target temperature of %.1f C: outside the encodable range", value);
    return;
  }
  this->parent_->set_target_temperature_decidegrees(static_cast<uint16_t>(decidegrees));
}

// Minutes in, seconds out -- see VolcanoAutoShutoffDurationNumber in
// volcano.h for why this entity carries minutes. Same overflow guard and
// same deferral of the safety bound as above.
void VolcanoAutoShutoffDurationNumber::control(float value) {
  long seconds = lroundf(value) * 60;
  if (seconds < 0 || seconds > UINT16_MAX) {
    ESP_LOGW(TAG, "Ignoring auto-shutoff duration of %.0f min: outside the encodable range", value);
    return;
  }
  this->parent_->set_auto_shutoff_duration_seconds(static_cast<uint16_t>(seconds));
}

// Neither switch publishes here: the write only asks, and the status/flags
// register (STATE-008) is what says whether the device actually did it.
// decode_status_() above publishes when that arrives.
void VolcanoHeaterSwitch::write_state(bool state) {
  if (state) {
    this->parent_->turn_heater_on();
  } else {
    this->parent_->turn_heater_off();
  }
}

void VolcanoPumpSwitch::write_state(bool state) {
  if (state) {
    this->parent_->turn_pump_on();
  } else {
    this->parent_->turn_pump_off();
  }
}

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
