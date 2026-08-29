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
test: $(BUILD)/volcano_device_test $(BUILD)/display_register_write_queue_test $(BUILD)/static_read_queue_test \
      $(BUILD)/wire_format_test
	$(BUILD)/volcano_device_test
	$(BUILD)/display_register_write_queue_test
	$(BUILD)/static_read_queue_test
	$(BUILD)/wire_format_test

$(BUILD)/volcano_device_test: volcano_device_test.cpp fakes/volcano_ble_client.h \
                              fakes/esphome/core/optional.h fakes/esphome/core/hal.h fakes/esphome/core/log.h \
                              ../volcano_device.h ../volcano_device.cpp ../volcano_ble_client_observer.h | $(BUILD)
	cp ../volcano_device.cpp $(BUILD)/volcano_device.cpp
	$(CXX) $(CXXFLAGS) volcano_device_test.cpp $(BUILD)/volcano_device.cpp -o $@

# display_register_write_queue.h, static_read_queue.h and wire_format.h each
# have no BLE/ESP-IDF dependency of their own (unlike volcano_ble_client.h,
# which they are part of), so they build directly against the real headers
# with no staging trick needed -- see each header's own comment.
$(BUILD)/display_register_write_queue_test: display_register_write_queue_test.cpp \
                                            fakes/esphome/core/optional.h \
                                            ../display_register_write_queue.h ../volcano_ble_client_observer.h | $(BUILD)
	$(CXX) $(CXXFLAGS) display_register_write_queue_test.cpp -o $@

$(BUILD)/static_read_queue_test: static_read_queue_test.cpp ../static_read_queue.h | $(BUILD)
	$(CXX) $(CXXFLAGS) static_read_queue_test.cpp -o $@

$(BUILD)/wire_format_test: wire_format_test.cpp ../wire_format.h | $(BUILD)
	$(CXX) $(CXXFLAGS) wire_format_test.cpp -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
