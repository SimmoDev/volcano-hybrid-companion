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
// This is the first working increment of the BLE communication layer: it
// proves the connect / resolve / subscribe / read / decode path end to end
// on a single characteristic, the status/flags register (CHAR-008 in
// docs/protocol/characteristics.md, decoded in
// docs/protocol/state-model.md#state-008-statusflags-register-partial). It
// issues no writes to any characteristic -- nothing here can actuate the
// heater or pump.
//
// Connection lifecycle follows ADR-0007
// (docs/decisions/ADR-0007-ble-connection-lifecycle.md): the parent
// ble_client holds a persistent connection and reconnects on its own, so
// this component re-resolves the characteristic and re-reads its value on
// every fresh connection (CONN-002, ADR-0007) rather than assuming state
// carries over, and treats heater/pump state as unknown while disconnected.
//
// TODO(volcano-component): the remaining characteristics, the full Volcano
// device state model, and the hardware-independent interface for control
// interfaces (all per ADR-0002) are not yet implemented -- this component
// currently only logs decoded state.
class VolcanoComponent : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                            esp_ble_gattc_cb_param_t *param) override;

 protected:
  void decode_status_(const uint8_t *value, uint16_t value_len);

  // Handle of the status/flags register (CHAR-008), resolved by UUID after
  // each connection. Zero while unresolved/disconnected.
  uint16_t status_handle_{0};
};

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
