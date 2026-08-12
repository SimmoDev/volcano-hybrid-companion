#pragma once

// Stands in for esphome/core/hal.h. VolcanoDevice::now_() falls back to
// esphome::millis() only when no clock has been injected via set_clock();
// every test here injects a deterministic clock, so this never needs a real
// implementation -- it exists only so volcano_device.cpp's #include resolves
// and links.

#include <cstdint>

namespace esphome {

inline uint32_t millis() { return 0; }

}  // namespace esphome
