#pragma once

// Mirrors the real esphome/core/optional.h exactly (a std::optional alias) --
// duplicated here rather than pulled from an installed ESPHome checkout so
// these tests depend on nothing but a C++17 compiler, per ADR-0009's "no
// ESPHome runtime present" requirement for VolcanoDevice.

#include <optional>

namespace esphome {

using std::make_optional;
using std::nullopt;
using std::nullopt_t;
using std::optional;

}  // namespace esphome
