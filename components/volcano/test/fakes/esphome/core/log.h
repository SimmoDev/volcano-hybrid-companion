#pragma once

// Stands in for esphome/core/log.h. VolcanoDevice only uses ESP_LOGW(), to
// warn when a set_*() call is made with no BLE client attached -- not
// something these tests assert on, so a no-op is enough to let
// volcano_device.cpp compile without ESPHome's real logging machinery.

#define ESP_LOGW(tag, ...) ((void) 0)
