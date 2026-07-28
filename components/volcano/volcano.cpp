#include "volcano.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

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
// CHAR-014: target temperature (SVC-006). Only the read/notify side is used
// here; writing it is CMD-001 and not yet implemented.
static const char *const TARGET_TEMP_CHARACTERISTIC_UUID = "10110003-5354-4f52-5a26-4249434b454c";

// STATE-008: bit 5 is set whenever the heater is on, clear when off.
static const uint16_t STATUS_BIT_HEATER_ON = 0x0020;
// STATE-008: bit 12 marks the pump specifically. Bit 13 is also pulsed by
// the vibration alert for about a second, so it must not be used to detect
// the pump.
static const uint16_t STATUS_BIT_PUMP_ON = 0x1000;

// CMD-003: the lowest auto-shutoff duration confirmed accepted, read back
// unchanged, loaded at the next arming, and honoured through to an actual
// expiry. Values below this are unverified -- 0 in particular may mean
// "disabled" on this device -- so set_auto_shutoff_duration_seconds()
// refuses them rather than writing an untested value (ADR-0005).
static const uint16_t MIN_AUTO_SHUTOFF_DURATION_SECONDS = 60;

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
  ESP_LOGCONFIG(TAG, "    current temperature (CHAR-013), target temperature (CHAR-014).");
  ESP_LOGCONFIG(TAG, "  Write: auto-shutoff duration (CHAR-017), minimum %u s.", MIN_AUTO_SHUTOFF_DURATION_SECONDS);
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
      this->pending_subscriptions_ = 0;
      ESP_LOGI(TAG, "Disconnected; heater/pump/countdown/temperature state unknown");
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

      // Nothing to subscribe to: nothing else will ever drive node_state to
      // ESTABLISHED for this connection, so declare it done here instead of
      // leaving the parent waiting on a subscription that will never arrive.
      if (this->pending_subscriptions_ == 0) {
        this->node_state = espbt::ClientState::ESTABLISHED;
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      uint16_t handle = param->reg_for_notify.handle;
      if (handle != this->status_handle_ && handle != this->countdown_handle_ &&
          handle != this->current_temp_handle_ && handle != this->target_temp_handle_)
        break;

      // node_state must not become ESTABLISHED until every subscription
      // issued in ESP_GATTC_SEARCH_CMPL_EVT above has been accounted for --
      // see the pending_subscriptions_ comment in volcano.h for why an
      // early ESTABLISHED here is a use-after-free, not just a race.
      if (this->pending_subscriptions_ > 0)
        this->pending_subscriptions_--;
      if (this->pending_subscriptions_ == 0)
        this->node_state = espbt::ClientState::ESTABLISHED;

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
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Reading handle 0x%04x failed, status=%d", param->read.handle, param->read.status);
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
          ESP_LOGI(TAG, "Auto-shutoff duration confirmed: %u s", seconds);
        }
      } else if (param->read.handle == this->current_temp_handle_) {
        this->decode_current_temperature_(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->target_temp_handle_) {
        this->decode_target_temperature_(param->read.value, param->read.value_len);
      }
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
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.handle != this->duration_handle_)
        break;
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Auto-shutoff duration write failed, status=%d", param->write.status);
        break;
      }
      auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                            this->duration_handle_, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_read_char(duration) failed, status=%d", status);
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
}

void VolcanoComponent::decode_countdown_(const uint8_t *value, uint16_t value_len) {
  // STATE-005: 2-byte little-endian seconds.
  if (value_len < 2) {
    ESP_LOGW(TAG, "Auto-shutoff countdown value too short (%u bytes)", value_len);
    return;
  }
  uint16_t seconds = encode_uint16(value[1], value[0]);
  ESP_LOGI(TAG, "Auto-shutoff countdown: %u s", seconds);
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
  // true 0 degC reading, so it must never be logged as a temperature.
  if (raw == 0) {
    ESP_LOGI(TAG, "Current temperature: no reading (below 40 C, heater off)");
  } else {
    ESP_LOGI(TAG, "Current temperature: %.1f C", raw / 10.0f);
  }
}

void VolcanoComponent::decode_target_temperature_(const uint8_t *value, uint16_t value_len) {
  uint32_t raw;
  if (!decode_decidegrees_c(value, value_len, &raw)) {
    ESP_LOGW(TAG, "Target temperature value too short (%u bytes)", value_len);
    return;
  }
  ESP_LOGI(TAG, "Target temperature: %.1f C", raw / 10.0f);
}

void VolcanoComponent::set_auto_shutoff_duration_seconds(uint16_t seconds) {
  if (seconds < MIN_AUTO_SHUTOFF_DURATION_SECONDS) {
    ESP_LOGW(TAG, "Refusing to set auto-shutoff duration to %u s: below the %u s floor confirmed accepted (CMD-003)",
             seconds, MIN_AUTO_SHUTOFF_DURATION_SECONDS);
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

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
