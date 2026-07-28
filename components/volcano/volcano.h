#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"

#include <esp_gattc_api.h>

namespace esphome {
namespace volcano {

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
// docs/protocol/commands.md). CMD-003 is Confirmed down to 60 seconds --
// accepted, read back unchanged, loaded at the next arming, and honoured
// through to an actual expiry -- so this refuses anything below that floor
// rather than writing an untested value: 0 in particular is unverified and
// may mean "disabled" on this device, which would silently remove the only
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
// Connection lifecycle follows ADR-0007
// (docs/decisions/ADR-0007-ble-connection-lifecycle.md): the parent
// ble_client holds a persistent connection and reconnects on its own, so
// this component re-resolves every characteristic and re-reads its value
// on every fresh connection (CONN-002, ADR-0007) rather than assuming
// state carries over, and treats heater/pump state as unknown while
// disconnected.
//
// TODO(volcano-component): the remaining characteristics, the full Volcano
// device state model, and the hardware-independent interface for control
// interfaces (all per ADR-0002) are not yet implemented -- this component
// currently only logs decoded state and writes the characteristics
// documented above.
class VolcanoComponent : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                            esp_ble_gattc_cb_param_t *param) override;

  // Writes CMD-003's confirmed 2-byte-seconds encoding to the auto-shutoff
  // duration characteristic (see the class comment above for the confirmed
  // floor this refuses to go below). A no-op, logged, if `seconds` is
  // below that floor or no connection is established.
  void set_auto_shutoff_duration_seconds(uint16_t seconds);

  // CMD-006 through CMD-009: each writes `0x00` to its trigger
  // characteristic. A no-op, logged, if no connection is established.
  void turn_heater_on();
  void turn_heater_off();
  void turn_pump_on();
  void turn_pump_off();

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
