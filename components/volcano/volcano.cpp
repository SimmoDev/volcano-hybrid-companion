#include "volcano.h"
#include "esphome/core/log.h"

namespace esphome {
namespace volcano {

static const char *const TAG = "volcano";

void VolcanoComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Volcano component...");
  // TODO(volcano-component): initialise the BLE communication layer here
  // once it exists (ADR-0002).
}

void VolcanoComponent::loop() {
  // TODO(volcano-component): drive BLE communication and state model
  // updates here once they exist (ADR-0002).
}

void VolcanoComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Volcano:");
  ESP_LOGCONFIG(TAG, "  No behaviour implemented yet (scaffold only).");
}

}  // namespace volcano
}  // namespace esphome
