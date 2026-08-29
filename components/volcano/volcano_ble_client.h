#pragma once

#include "display_register_write_queue.h"
#include "static_read_queue.h"
#include "volcano_ble_client_observer.h"

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <cstdint>
#include <string>

#include <esp_gattc_api.h>

namespace esphome {
namespace volcano {

// The BLE communication layer (ADR-0002, ADR-0009). Owns every service,
// characteristic, GATT event and wire encoding/decoding detail for the
// Volcano Hybrid -- none of it appears in VolcanoDevice's interface.
// Reports decoded domain values upward through the observer interface it
// declares, and accepts domain values to write, doing wire encoding and
// range-checking internally. Has no knowledge of VolcanoDevice.
//
// Not itself a Component or ble_client::BLEClientNode -- VolcanoComponent
// owns both, and forwards its gattc_event_handler() callback into this
// class's method of the same name.
class VolcanoBleClient {
 public:
  // Not owned by this class. node_state is VolcanoComponent's own
  // ble_client::BLEClientNode::node_state -- this class does not inherit
  // that type, so VolcanoComponent hands it a pointer rather than exposing
  // its own copy. Set to ESTABLISHED at the same point the previous,
  // unsplit component set it: once every subscription issued during
  // service discovery has settled (see pending_subscriptions_ below for
  // why that timing matters).
  void set_client(ble_client::BLEClient *client) { this->client_ = client; }
  void set_observer(VolcanoBleClientObserver *observer) { this->observer_ = observer; }
  void set_node_state(esphome::esp32_ble_tracker::ClientState *node_state) { this->node_state_ = node_state; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

  // Each writes the encoding CMD-001/002/003/004/005/006-009/010 confirm
  // accepted, and returns whether a write was actually issued -- false
  // (logged) if refused for being outside a confirmed-accepted range, or if
  // the relevant characteristic is not currently resolved (not connected).
  // A `true` return means only that the write was queued: the observer's
  // on_write_failed() or the matching on_*() report is what confirms it
  // actually reached the device, per ADR-0009.
  //
  // write_auto_shutoff_duration()/write_led_brightness() additionally refuse
  // (false, logged) until the static-read sweep has completed -- see
  // static_sweep_done_ below for why.
  bool write_target_temperature(float celsius);
  bool write_heater(bool on);
  bool write_pump(bool on);
  bool write_auto_shutoff_duration(uint16_t seconds);
  bool write_led_brightness(uint8_t percent);
  bool write_vibration(bool enabled);
  bool write_display_on_cooling(bool enabled);
  bool write_display_units_fahrenheit(bool fahrenheit);

 private:
  void decode_status_(const uint8_t *value, uint16_t value_len);
  void decode_countdown_(const uint8_t *value, uint16_t value_len);
  void decode_current_temperature_(const uint8_t *value, uint16_t value_len);
  // `from_read` distinguishes the two things that ever read this
  // characteristic (the initial per-connection read and a write's own
  // read-back) from a notification -- see
  // VolcanoBleClientObserver::on_target_temperature() for why that matters.
  void decode_target_temperature_(const uint8_t *value, uint16_t value_len, bool from_read);
  void decode_vibration_(const uint8_t *value, uint16_t value_len);
  void decode_display_register_(const uint8_t *value, uint16_t value_len);
  void decode_hours_(const uint8_t *value, uint16_t value_len);
  void decode_minutes_(const uint8_t *value, uint16_t value_len);
  // Both read once per connection and again as a write's own read-back;
  // neither notifies. Kept as named methods like every other characteristic's
  // decode rather than inlined at the ESP_GATTC_READ_CHAR_EVT call site.
  void decode_auto_shutoff_duration_(const uint8_t *value, uint16_t value_len);
  void decode_led_brightness_(const uint8_t *value, uint16_t value_len);
  // `report` is bound to the specific on_*_version()/on_serial_number()/etc.
  // observer method for the characteristic being decoded -- the five
  // device-information strings share this one decode body. `name` is for
  // the log line only.
  void decode_text_(const uint8_t *value, uint16_t value_len,
                    void (VolcanoBleClientObserver::*report)(const std::string &), const char *name);

  void read_auto_shutoff_duration_();
  bool write_trigger_(uint16_t handle, const char *name);

  // None of the device-information characteristics notify, so each is read
  // once per connection, one at a time -- a GATT client has only a small
  // number of outstanding reads available, and this queue shares that
  // budget with the reads the subscriptions above already issue. Its
  // completion is what ADR-0009's on_ready() actually waits for.
  void queue_static_reads_();
  void issue_next_static_read_();

  ble_client::BLEClient *client_{nullptr};
  VolcanoBleClientObserver *observer_{nullptr};
  esphome::esp32_ble_tracker::ClientState *node_state_{nullptr};

  // Handles resolved by UUID after each connection. Zero while
  // unresolved/disconnected.
  uint16_t status_handle_{0};        // CHAR-008: status/flags register.
  uint16_t countdown_handle_{0};     // CHAR-016: auto-shutoff countdown.
  uint16_t duration_handle_{0};      // CHAR-017: auto-shutoff duration.
  uint16_t current_temp_handle_{0};  // CHAR-013: current (actual) temperature.
  uint16_t target_temp_handle_{0};   // CHAR-014: target temperature.
  uint16_t heater_on_handle_{0};     // CHAR-018: heater on trigger.
  uint16_t heater_off_handle_{0};    // CHAR-019: heater off trigger.
  uint16_t pump_on_handle_{0};       // CHAR-020: pump on trigger.
  uint16_t pump_off_handle_{0};      // CHAR-021: pump off trigger.
  // Device information, all read-only in practice. CHAR-007, CHAR-024 and
  // CHAR-025 are writable on the device, and must never be written: CHAR-007
  // in particular would overwrite a real unit's serial number.
  uint16_t firmware_version_handle_{0};      // CHAR-005: firmware version.
  uint16_t ble_firmware_version_handle_{0};  // CHAR-006: firmware BLE version.
  uint16_t serial_number_handle_{0};         // CHAR-007: serial number.
  uint16_t power_supply_handle_{0};          // CHAR-024: power supply rating.
  uint16_t product_line_handle_{0};          // CHAR-025: product line name.
  // The heater-runtime meter, notify-capable and so subscribed rather than
  // read once. Both advance only while the heater is on.
  uint16_t hours_handle_{0};             // CHAR-022: hours of operation.
  uint16_t minutes_handle_{0};           // CHAR-023: minutes of operation.
  uint16_t led_brightness_handle_{0};    // CHAR-015: LED brightness.
  uint16_t vibration_handle_{0};         // CHAR-010: vibration setting.
  uint16_t display_register_handle_{0};  // CHAR-009: display/units register.

  // See display_register_write_queue.h for why this handle needs a FIFO
  // rather than tracking a single "most recent write" field.
  DisplayRegisterWriteQueue display_register_pending_writes_;

  // See static_read_queue.h for the ordering bookkeeping behind the
  // once-per-connection reads; the esp_ble_gattc_read_char() calls
  // themselves stay in issue_next_static_read_().
  StaticReadQueue static_reads_;

  // Last decoded display-on-cooling state, or -1 before the first read.
  // Its register notifies on every 1 degC change of current temperature
  // (STATE-009), not only when the setting changes, so this exists to
  // keep the log to actual changes rather than a line every few seconds
  // throughout a heating or cooling run.
  int8_t display_on_cooling_log_state_{-1};

  // Number of ESP_GATTC_REG_FOR_NOTIFY_EVT callbacks still outstanding.
  // node_state must not become ESTABLISHED until this reaches zero: the
  // parent ble_client releases its GATT service/characteristic cache once
  // every node reports ESTABLISHED, and the CCCD-descriptor lookup each
  // pending subscription still needs (performed internally by
  // esp32_ble_client::BLEClientBase for every REG_FOR_NOTIFY_EVT) reads
  // that same cache -- reporting ESTABLISHED early releases it out from
  // under a subscription still in flight and crashes.
  uint8_t pending_subscriptions_{0};

  // Set once issue_next_static_read_() has worked through every queued
  // static read for this connection (the same point that fires on_ready()),
  // cleared on disconnect and again at the start of the next connection's
  // service discovery. Exists so write_auto_shutoff_duration() and
  // write_led_brightness() can refuse to write while it is false: both
  // characteristics are also static-read queue members (duration_handle_ and
  // led_brightness_handle_), and a write's own read-back
  // (read_auto_shutoff_duration_(), and the equivalent inline read for LED
  // brightness) issues a second, independent esp_ble_gattc_read_char() on
  // the same handle. ESP_GATTC_READ_CHAR_EVT carries no request identity, so
  // StaticReadQueue::advance_if_current()'s handle-and-position match cannot
  // tell that second read's response apart from the sweep's own -- whichever
  // response lands first gets credited to the sweep's current slot
  // regardless of which call actually produced it, which can silently skip a
  // real static read and fire on_ready() one read early. Refusing the write
  // until the sweep is done removes the second read entirely rather than
  // trying to make the matching logic tell the two apart.
  bool static_sweep_done_{false};
};

}  // namespace volcano
}  // namespace esphome
