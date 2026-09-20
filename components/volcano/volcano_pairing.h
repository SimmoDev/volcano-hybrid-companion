#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text/text.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preference_backend.h"

#include "pairing.h"

#include <string>

namespace esphome {
namespace volcano {

class VolcanoPairing;

// The paired Volcano's BLE address as an editable entity, for correcting or
// setting it from a web_server page or Home Assistant (ADR-0013). Shows an
// empty string while unpaired; setting it to an empty string forgets the
// current unit.
class VolcanoAddressText : public text::Text, public Parented<VolcanoPairing> {
 protected:
  void control(const std::string &value) override;
};

// Establishes which Volcano the Dial controls, ADR-0013's runtime
// replacement for the compile-time `mac_address`. It listens to the
// tracker's advertisements, recognises a Volcano Hybrid from them, stores
// the address it settles on, and points the `ble_client` at it. From then on
// the `ble_client` behaves exactly as it did with a compile-time address.
//
// This is Dial-side plumbing, not a Volcano domain concept, so it sits beside
// VolcanoComponent rather than inside VolcanoDevice (ADR-0011's Pairing page
// note): it never touches a characteristic, and VolcanoDevice never learns
// how the address was chosen. The decisions live in PairingSelector
// (pairing.h); this class is the ESPHome half -- the tracker listener, the
// preference storage, the `ble_client` and the entities.
//
// Not paired means the `ble_client` has no address, so it never connects: a
// Dial that has never been paired boots and runs its local UI, and only
// Volcano control waits (ADR-0013, standalone during onboarding).
class VolcanoPairing : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  using State = PairingSelector::State;

  void setup() override;
  void dump_config() override;
  // After the ble_client (AFTER_BLUETOOTH), so this class has the last word
  // on the address the client is left with.
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH - 1.0f; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void set_ble_client(ble_client::BLEClient *client) { this->ble_client_ = client; }
  void set_scan_window_ms(uint32_t ms) { this->selector_.set_scan_window_ms(ms); }
  void set_address_text(VolcanoAddressText *text) { this->address_text_ = text; }
  void set_status_text_sensor(text_sensor::TextSensor *sensor) { this->status_text_sensor_ = sensor; }

  // The operations a control interface offers, callable from YAML lambdas
  // the same way the Connections page calls set_enabled() -- ESPHome has no
  // action for them. All of them act at once instead of waiting for the next
  // periodic pass, so a page reflects a press without a visible lag.
  //
  // Discards what was seen and opens a fresh scan window. Refused (false)
  // while paired: forget() first.
  bool rescan();
  // Picks the `index`th candidate while several are on offer.
  bool select(size_t index);
  // Drops the paired Volcano and starts looking again, for a replaced unit.
  void forget();
  // Pairs with an address typed in: "" forgets, a valid address pairs with
  // it, anything else is refused (false) and changes nothing.
  bool set_address_from_text(const std::string &text);

  State state() const { return this->selector_.state(); }
  bool paired() const { return this->selector_.state() == State::PAIRED; }
  std::string address_str() const;
  size_t candidate_count() const { return this->selector_.candidate_count(); }
  const char *candidate_serial(size_t index) const { return this->selector_.candidate_serial(index); }
  int candidate_rssi(size_t index) const { return this->selector_.candidate_rssi(index); }
  std::string candidate_address_str(size_t index) const;
  // One line describing where pairing has got to, for the status entity.
  std::string status_text() const;

 protected:
  // The periodic pass: closes an expired scan window, then reconciles.
  void update_();
  // Persists a changed address, points the ble_client at it, and publishes
  // whatever the entities show.
  void sync_();
  void apply_address_();
  void publish_();

  PairingSelector selector_;
  ble_client::BLEClient *ble_client_{nullptr};
  ESPPreferenceObject preference_;

  VolcanoAddressText *address_text_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};

  // What is in storage, so a write happens only on a change.
  uint64_t saved_address_{0};
  // Last strings sent to the entities, so an unchanged value is not
  // republished (text and text_sensor do not dedupe themselves).
  std::string published_address_;
  std::string published_status_;
  bool published_{false};
};

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
