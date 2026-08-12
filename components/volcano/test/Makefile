# Host-side tests for VolcanoDevice -- see volcano_device_test.cpp and
# ADR-0009 (docs/decisions/ADR-0009-volcano-abstraction-layer-interface.md).
#
# volcano_device.cpp's own #include "volcano_ble_client.h" always resolves
# relative to volcano_device.cpp's own directory first -- true for any
# quoted #include, regardless of -I order -- so the real, BLE/ESP-IDF-
# dependent header next to it can't be shadowed with a search-path flag
# alone. volcano_device.cpp is staged into build/ next to fakes/'s
# volcano_ble_client.h instead, copied fresh from the real source on every
# build so there is no separately-maintained copy to drift out of date.
# volcano_device.h itself needs no such staging: it only reaches the
# already-portable volcano_ble_client_observer.h and esphome/core/optional.h.

CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I fakes -I ..
BUILD := build

.PHONY: test clean
test: $(BUILD)/volcano_device_test
	$(BUILD)/volcano_device_test

$(BUILD)/volcano_device_test: volcano_device_test.cpp fakes/volcano_ble_client.h \
                              fakes/esphome/core/optional.h fakes/esphome/core/hal.h fakes/esphome/core/log.h \
                              ../volcano_device.h ../volcano_device.cpp ../volcano_ble_client_observer.h | $(BUILD)
	cp ../volcano_device.cpp $(BUILD)/volcano_device.cpp
	$(CXX) $(CXXFLAGS) volcano_device_test.cpp $(BUILD)/volcano_device.cpp -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
