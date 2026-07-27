#include "volcano.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace volcano {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const TAG = "volcano";

// SVC-005 (settings/info service) and CHAR-008 (status/flags register),
// per docs/protocol/gatt-services.md and docs/protocol/characteristics.md.
// Resolved by UUID, never by handle: several characteristics on this device
// share an identical one-byte read/write shape, and writing to the wrong
// one actuates real hardware -- see docs/protocol/README.md's "What these
// findings apply to" section. This characteristic is read/notify only, but
// the same resolve-by-UUID discipline applies to it too.
static const char *const STATUS_SERVICE_UUID = "10100000-5354-4f52-5a26-4249434b454c";
static const char *const STATUS_CHARACTERISTIC_UUID = "1010000c-5354-4f52-5a26-4249434b454c";

// STATE-008: bit 5 is set whenever the heater is on, clear when off.
static const uint16_t STATUS_BIT_HEATER_ON = 0x0020;
// STATE-008: bit 12 marks the pump specifically. Bit 13 is also pulsed by
// the vibration alert for about a second, so it must not be used to detect
// the pump.
static const uint16_t STATUS_BIT_PUMP_ON = 0x1000;

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
  ESP_LOGCONFIG(TAG, "  Read-only: status/flags register (CHAR-008) only.");
}

void VolcanoComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      // ADR-0007: device state is marked unknown while disconnected.
      this->status_handle_ = 0;
      ESP_LOGI(TAG, "Disconnected; heater/pump state unknown");
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service discovery failed, status=%d", param->search_cmpl.status);
        break;
      }
      auto *chr = this->parent()->get_characteristic(espbt::ESPBTUUID::from_raw(STATUS_SERVICE_UUID),
                                                      espbt::ESPBTUUID::from_raw(STATUS_CHARACTERISTIC_UUID));
      if (chr == nullptr) {
        ESP_LOGW(TAG, "Status/flags register (CHAR-008) not found on device");
        break;
      }
      this->status_handle_ = chr->handle;

      // CONN-002: nothing is pushed on subscribing, so the initial value is
      // read explicitly once subscription completes -- see the
      // ESP_GATTC_REG_FOR_NOTIFY_EVT case below.
      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                       this->parent()->get_remote_bda(), this->status_handle_);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->status_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Subscribing to status/flags register failed, status=%d", param->reg_for_notify.status);
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                            this->status_handle_, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_read_char failed, status=%d", status);
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.handle != this->status_handle_)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Reading status/flags register failed, status=%d", param->read.status);
        break;
      }
      this->decode_status_(param->read.value, param->read.value_len);
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->status_handle_)
        break;
      this->decode_status_(param->notify.value, param->notify.value_len);
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

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
