// Host-side tests for pairing.h (components/volcano/pairing.h): no ESP-IDF, no
// ESPHome runtime, no real BLE client or hardware needed -- the header has no
// BLE/ESP-IDF dependency, so it builds directly against the real header with
// no fakes required. See test/Makefile.
//
// No external test framework, matching wire_format_test.cpp: a handful of
// CHECK()s and a summary line is enough for this.
//
// What these guard is the identification rule and the one/several/none
// decision ADR-0013 records: a wrong match here adopts a stranger's device,
// or an S&B product that is not a Volcano Hybrid, as the one the Dial drives.

#include "pairing.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace esphome::volcano;
using State = PairingSelector::State;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char *expr, const char *file, int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
  }
}

constexpr uint64_t ADDR_A = 0xC0A1B2C3D4E5ULL;
constexpr uint64_t ADDR_B = 0xC0A1B2C3D4E6ULL;
constexpr uint64_t ADDR_C = 0xC0A1B2C3D4E7ULL;
constexpr uint32_t WINDOW = 10000;

PairingSelector new_selector() {
  PairingSelector selector;
  selector.set_scan_window_ms(WINDOW);
  return selector;
}

// A full match: the advertisement's serial and the scan response's name,
// seen under the same address.
void see_volcano(PairingSelector &s, uint64_t address, const char *serial, int rssi) {
  s.observe_serial(address, serial, rssi);
  s.observe_name(address, VOLCANO_ADVERTISED_NAME, rssi);
}

}  // namespace

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// ADV-001: 10 ASCII bytes follow the company ID; the rest of the 24-byte
// payload is undecoded and must not be read as part of the serial.
void test_serial_from_manufacturer_data() {
  const uint8_t payload[] = {'V', 'H', '0', '1', '2', '3', '4', '5', '6', '7', 0xAA, 0xBB, 0xCC};
  std::string serial;
  CHECK(serial_from_manufacturer_data(payload, sizeof(payload), serial));
  CHECK(serial == "VH01234567");

  // Exactly the serial and nothing after it is still enough.
  CHECK(serial_from_manufacturer_data(payload, SERIAL_LENGTH, serial));

  // One byte short, and nothing at all.
  CHECK(!serial_from_manufacturer_data(payload, SERIAL_LENGTH - 1, serial));
  CHECK(!serial_from_manufacturer_data(payload, 0, serial));
  CHECK(!serial_from_manufacturer_data(nullptr, 24, serial));

  // A non-printable byte inside the serial's span means this is some other
  // S&B product's manufacturer data, not a serial number.
  const uint8_t binary[] = {'V', 'H', 0x01, '3', '4', '5', '6', '7', '8', '9'};
  CHECK(!serial_from_manufacturer_data(binary, sizeof(binary), serial));
  const uint8_t high[] = {'V', 'H', '0', '1', '2', 0xFF, '4', '5', '6', '7'};
  CHECK(!serial_from_manufacturer_data(high, sizeof(high), serial));
}

void test_format_address() {
  CHECK(format_address(0xC0A1B2C3D4E5ULL) == "C0:A1:B2:C3:D4:E5");
  // Leading zero bytes are kept -- a fixed six-octet form, not a number.
  CHECK(format_address(0x0000000000ABULL) == "00:00:00:00:00:AB");
  CHECK(format_address(0) == "00:00:00:00:00:00");
}

void test_parse_address() {
  uint64_t address = 0;
  CHECK(parse_address("C0:A1:B2:C3:D4:E5", address));
  CHECK(address == 0xC0A1B2C3D4E5ULL);

  // Either case.
  CHECK(parse_address("c0:a1:b2:c3:d4:e5", address));
  CHECK(address == 0xC0A1B2C3D4E5ULL);

  // A round trip through format_address().
  CHECK(parse_address(format_address(ADDR_B), address));
  CHECK(address == ADDR_B);

  // Everything else is refused, and leaves `address` alone.
  address = 0x1234;
  CHECK(!parse_address("", address));
  CHECK(!parse_address("C0:A1:B2:C3:D4", address));
  CHECK(!parse_address("C0:A1:B2:C3:D4:E5:F6", address));
  CHECK(!parse_address("C0-A1-B2-C3-D4-E5", address));
  CHECK(!parse_address("C0A1B2C3D4E5FFFFF", address));
  CHECK(!parse_address("C0:A1:B2:C3:D4:GG", address));
  CHECK(!parse_address(" C0:A1:B2:C3:D4:E5", address));
  CHECK(address == 0x1234);

  // 0 means "no address" to ble_client, so it can never be a target.
  CHECK(!parse_address("00:00:00:00:00:00", address));
}

// ADR-0013: a Dial that has been paired connects straight away, with no scan.
void test_begin_with_stored_address_is_paired() {
  PairingSelector s = new_selector();
  s.begin(ADDR_A, 0);
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_A);
  // No window to close.
  CHECK(!s.process(WINDOW * 10));
}

void test_begin_without_stored_address_scans() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  CHECK(s.state() == State::SCANNING);
  CHECK(s.address() == 0);
}

// ADR-0013: exactly one match is adopted silently, once the window closes --
// not the moment it is seen, so a second unit appearing a moment later is
// still noticed.
void test_one_match_is_adopted_when_the_window_closes() {
  PairingSelector s = new_selector();
  s.begin(0, 1000);
  see_volcano(s, ADDR_A, "VH01234567", -60);

  CHECK(s.candidate_count() == 1);
  CHECK(!s.process(1000 + WINDOW - 1));
  CHECK(s.state() == State::SCANNING);

  CHECK(s.process(1000 + WINDOW));
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_A);
}

void test_several_matches_are_offered_strongest_first() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  see_volcano(s, ADDR_A, "VH00000001", -80);
  see_volcano(s, ADDR_B, "VH00000002", -50);
  see_volcano(s, ADDR_C, "VH00000003", -65);

  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::CHOOSING);
  CHECK(s.address() == 0);
  CHECK(s.candidate_count() == 3);
  CHECK(s.candidate_address(0) == ADDR_B);
  CHECK(s.candidate_address(1) == ADDR_C);
  CHECK(s.candidate_address(2) == ADDR_A);
  CHECK(std::string(s.candidate_serial(0)) == "VH00000002");
  CHECK(s.candidate_rssi(0) == -50);
  CHECK(s.candidate_address(3) == 0);
  CHECK(std::string(s.candidate_serial(3)).empty());

  CHECK(s.select(1));
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_C);
}

void test_no_match_is_not_found() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::NOT_FOUND);
  CHECK(s.address() == 0);
  CHECK(s.candidate_count() == 0);
  // Terminal until asked to scan again.
  CHECK(!s.process(WINDOW * 5));
  CHECK(s.state() == State::NOT_FOUND);
}

// ADV-001: the company ID is shared by every S&B product, so the serial on
// its own does not make a Volcano Hybrid; nor does the name on its own tell
// the Dial which unit it is looking at.
void test_a_serial_without_the_name_is_not_a_volcano() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  s.observe_serial(ADDR_A, "CR01234567", -50);
  CHECK(s.candidate_count() == 0);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::NOT_FOUND);
}

void test_a_name_without_a_serial_is_not_a_candidate() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  s.observe_name(ADDR_A, VOLCANO_ADVERTISED_NAME, -50);
  CHECK(s.candidate_count() == 0);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::NOT_FOUND);
}

// The advertisement and the scan response are separate events, in either
// order, so what is seen for one address accumulates.
void test_serial_and_name_combine_in_either_order() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  s.observe_name(ADDR_A, VOLCANO_ADVERTISED_NAME, -70);
  CHECK(s.candidate_count() == 0);
  s.observe_serial(ADDR_A, "VH01234567", -60);
  CHECK(s.candidate_count() == 1);
  // The strongest reading seen is the one kept.
  CHECK(s.candidate_rssi(0) == -60);
}

// The name and the serial must belong to the same address: a Crafty's serial
// and a Volcano's name seen from two different units do not add up to one.
void test_serial_and_name_from_different_addresses_do_not_combine() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  s.observe_serial(ADDR_A, "CR01234567", -50);
  s.observe_name(ADDR_B, VOLCANO_ADVERTISED_NAME, -50);
  CHECK(s.candidate_count() == 0);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::NOT_FOUND);
}

void test_other_names_are_ignored() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  s.observe_serial(ADDR_A, "VH01234567", -50);
  s.observe_name(ADDR_A, "S&B CRAFTY+", -50);
  s.observe_name(ADDR_A, "S&B VOLCANO", -50);
  s.observe_name(ADDR_A, "", -50);
  CHECK(s.candidate_count() == 0);
}

// Seeing the same unit again and again, as a scan does, is one candidate, not
// many.
void test_repeat_sightings_are_one_candidate() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  for (int i = 0; i < 200; i++)
    see_volcano(s, ADDR_A, "VH01234567", -70 + (i % 5));
  CHECK(s.candidate_count() == 1);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_A);
}

// Half-seen devices do not make the window ambiguous: only full matches count
// towards "one" or "several".
void test_unconfirmed_devices_do_not_count_towards_several() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  see_volcano(s, ADDR_A, "VH01234567", -60);
  s.observe_serial(ADDR_B, "CR01234567", -40);
  s.observe_name(ADDR_C, VOLCANO_ADVERTISED_NAME, -40);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_A);
}

// Once the window has closed the list a chooser is showing must not change.
void test_sightings_after_the_window_closes_are_ignored() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  see_volcano(s, ADDR_A, "VH00000001", -60);
  see_volcano(s, ADDR_B, "VH00000002", -70);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::CHOOSING);

  see_volcano(s, ADDR_C, "VH00000003", -30);
  CHECK(s.candidate_count() == 2);
  CHECK(s.candidate_address(0) == ADDR_A);
}

// More S&B devices than there are slots: the ones seen first are kept, and
// the extras are dropped rather than displacing them.
void test_candidate_table_is_bounded() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  for (size_t i = 0; i < MAX_PAIRING_CANDIDATES; i++)
    see_volcano(s, 0xC00000000001ULL + i, "VH00000000", -60);
  see_volcano(s, 0xC000000000FFULL, "VH99999999", -20);
  CHECK(s.candidate_count() == MAX_PAIRING_CANDIDATES);
  CHECK(s.process(WINDOW));
  for (size_t i = 0; i < s.candidate_count(); i++)
    CHECK(s.candidate_address(i) != 0xC000000000FFULL);
}

// The window is measured on a clock that can wrap.
void test_window_survives_clock_wraparound() {
  PairingSelector s = new_selector();
  s.begin(0, 0xFFFFFFFFu - 4000);
  see_volcano(s, ADDR_A, "VH01234567", -60);
  CHECK(!s.process(0xFFFFFFFFu));
  CHECK(!s.process(5000));
  CHECK(s.process(6000));
  CHECK(s.state() == State::PAIRED);
}

void test_select_only_while_choosing() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  see_volcano(s, ADDR_A, "VH00000001", -60);
  see_volcano(s, ADDR_B, "VH00000002", -70);
  // Still scanning: nothing to choose between yet.
  CHECK(!s.select(0));
  CHECK(s.process(WINDOW));
  // Out of range.
  CHECK(!s.select(2));
  CHECK(s.state() == State::CHOOSING);
  CHECK(s.select(0));
  // Already paired: a second select is refused.
  CHECK(!s.select(1));
  CHECK(s.address() == ADDR_A);
}

void test_rescan_from_not_found_and_choosing() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  CHECK(s.process(WINDOW));
  CHECK(s.state() == State::NOT_FOUND);

  CHECK(s.start_scan(WINDOW));
  CHECK(s.state() == State::SCANNING);
  see_volcano(s, ADDR_A, "VH00000001", -60);
  see_volcano(s, ADDR_B, "VH00000002", -70);
  CHECK(s.process(2 * WINDOW));
  CHECK(s.state() == State::CHOOSING);

  // A rescan starts from nothing, not from what the last window saw.
  CHECK(s.start_scan(2 * WINDOW));
  CHECK(s.candidate_count() == 0);
  see_volcano(s, ADDR_C, "VH00000003", -60);
  CHECK(s.process(3 * WINDOW));
  CHECK(s.address() == ADDR_C);
}

// A rescan must not be able to drop the device already paired with; that is
// forget()'s job.
void test_rescan_is_refused_while_paired() {
  PairingSelector s = new_selector();
  s.begin(ADDR_A, 0);
  CHECK(!s.start_scan(100));
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_A);
}

void test_forget_clears_and_scans() {
  PairingSelector s = new_selector();
  s.begin(ADDR_A, 0);
  s.forget(5000);
  CHECK(s.state() == State::SCANNING);
  CHECK(s.address() == 0);

  // The forgotten unit is still in range and is found again like any other;
  // the window is measured from the forget, not from boot.
  see_volcano(s, ADDR_B, "VH00000002", -60);
  CHECK(!s.process(WINDOW));
  CHECK(s.process(5000 + WINDOW));
  CHECK(s.address() == ADDR_B);
}

// The web_server / Home Assistant path: an address given directly replaces
// whatever state the selector was in.
void test_set_address_pairs_from_any_state() {
  PairingSelector s = new_selector();
  s.begin(0, 0);
  s.set_address(ADDR_A);
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_A);
  // ...including while a window would otherwise have gone on to close.
  CHECK(!s.process(WINDOW));
  CHECK(s.address() == ADDR_A);

  // Replaces one already paired.
  s.set_address(ADDR_B);
  CHECK(s.address() == ADDR_B);

  // 0 is not an address; it is ignored rather than clearing the pairing.
  s.set_address(0);
  CHECK(s.state() == State::PAIRED);
  CHECK(s.address() == ADDR_B);

  // And from CHOOSING.
  PairingSelector c = new_selector();
  c.begin(0, 0);
  see_volcano(c, ADDR_A, "VH00000001", -60);
  see_volcano(c, ADDR_B, "VH00000002", -70);
  CHECK(c.process(WINDOW));
  CHECK(c.state() == State::CHOOSING);
  c.set_address(ADDR_C);
  CHECK(c.state() == State::PAIRED);
  CHECK(c.address() == ADDR_C);
}

int main() {
  test_serial_from_manufacturer_data();
  test_format_address();
  test_parse_address();
  test_begin_with_stored_address_is_paired();
  test_begin_without_stored_address_scans();
  test_one_match_is_adopted_when_the_window_closes();
  test_several_matches_are_offered_strongest_first();
  test_no_match_is_not_found();
  test_a_serial_without_the_name_is_not_a_volcano();
  test_a_name_without_a_serial_is_not_a_candidate();
  test_serial_and_name_combine_in_either_order();
  test_serial_and_name_from_different_addresses_do_not_combine();
  test_other_names_are_ignored();
  test_repeat_sightings_are_one_candidate();
  test_unconfirmed_devices_do_not_count_towards_several();
  test_sightings_after_the_window_closes_are_ignored();
  test_candidate_table_is_bounded();
  test_window_survives_clock_wraparound();
  test_select_only_while_choosing();
  test_rescan_from_not_found_and_choosing();
  test_rescan_is_refused_while_paired();
  test_forget_clears_and_scans();
  test_set_address_pairs_from_any_state();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
