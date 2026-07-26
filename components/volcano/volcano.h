#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace volcano {

// VolcanoComponent is the root of the Volcano component defined in
// ADR-0002 (docs/decisions/ADR-0002-volcano-component-architecture.md).
//
// This is a scaffold only. It compiles and registers as a valid ESPHome
// component, but implements no behaviour yet. In particular:
//
//   TODO(volcano-component): BLE communication layer (device connection,
//     GATT services/characteristics, wire protocol encode/decode) belongs
//     here, kept internal to this component per ADR-0002. Do not add BLE
//     details anywhere control interfaces can see them.
//
//   TODO(volcano-component): Volcano device state model (current/target
//     temperature, heater state, pump state, connection state, and other
//     observed device state -- see docs/protocol/state-model.md) belongs
//     here as the single source of truth per ADR-0002.
//
//   TODO(volcano-component): domain-level commands and state accessors
//     exposed to control interfaces (e.g. set target temperature, heater
//     on/off) belong here, expressed in Volcano domain terms only -- no
//     BLE types or values in this public interface.
//
// Per ADR-0005, none of the above should be implemented until backed by
// a Confirmed (or explicitly marked experimental Probable) protocol
// finding under docs/protocol/.
class VolcanoComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
};

}  // namespace volcano
}  // namespace esphome
