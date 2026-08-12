#pragma once

// Test double for the real VolcanoBleClient (components/volcano/
// volcano_ble_client.h), reached by volcano_device.cpp's own
// #include "volcano_ble_client.h" once that file is staged into this
// fakes-adjacent build directory -- see test/Makefile for why staging is
// necessary rather than a plain -I override. VolcanoDevice only ever calls
// these eight write_*() methods on a VolcanoBleClient*, so that is the
// entire surface this fake needs to provide; none of the real class's BLE
// internals are reachable or relevant from VolcanoDevice's side of the seam.

#include <cstdint>

namespace esphome {
namespace volcano {

class VolcanoBleClient {
 public:
  // What the next write_*() call returns, simulating VolcanoBleClient's own
  // range-check/not-connected refusal (false) or a successfully queued
  // write (true, the default).
  bool next_write_result{true};

  bool write_target_temperature(float celsius) {
    this->last_target_temperature = celsius;
    return this->next_write_result;
  }
  bool write_heater(bool on) {
    this->last_heater = on;
    return this->next_write_result;
  }
  bool write_pump(bool on) {
    this->last_pump = on;
    return this->next_write_result;
  }
  bool write_auto_shutoff_duration(uint16_t seconds) {
    this->last_auto_shutoff_duration = seconds;
    return this->next_write_result;
  }
  bool write_led_brightness(uint8_t percent) {
    this->last_led_brightness = percent;
    return this->next_write_result;
  }
  bool write_vibration(bool enabled) {
    this->last_vibration = enabled;
    return this->next_write_result;
  }
  bool write_display_on_cooling(bool enabled) {
    this->last_display_on_cooling = enabled;
    return this->next_write_result;
  }
  bool write_display_units_fahrenheit(bool fahrenheit) {
    this->last_display_units_fahrenheit = fahrenheit;
    return this->next_write_result;
  }

  // The argument most recently passed to each write_*() above, so a test
  // can assert VolcanoDevice actually issued the write it should have.
  float last_target_temperature{};
  bool last_heater{};
  bool last_pump{};
  uint16_t last_auto_shutoff_duration{};
  uint8_t last_led_brightness{};
  bool last_vibration{};
  bool last_display_on_cooling{};
  bool last_display_units_fahrenheit{};
};

}  // namespace volcano
}  // namespace esphome
