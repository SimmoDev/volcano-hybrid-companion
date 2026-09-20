#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome {
namespace volcano {

// Pure discovery and pairing logic for finding which Volcano the Dial should
// control (ADR-0013's Notes, ADR-0011's Pairing page), split out of
// volcano_pairing.cpp so the identification rules and the
// one/several/none decision -- the part where a wrong match adopts a
// stranger's device or a Crafty in place of the Volcano -- are host-testable
// without ESPHome or a BLE stack, the same split wire_format.h makes. The
// tracker listener, the preference storage and the entity publishing stay in
// VolcanoPairing; only the decisions live here. See test/pairing_test.cpp.

// ADV-001: the advertisement's manufacturer data is keyed by Storz &
// Bickel's company ID, and its first SERIAL_LENGTH bytes after that ID are
// the unit's serial number as ASCII. The company ID alone does not identify
// a Volcano Hybrid -- it is shared by every S&B product -- so it is only half
// of the evidence; see VOLCANO_ADVERTISED_NAME.
inline constexpr uint16_t STORZ_BICKEL_COMPANY_ID = 1736;
inline constexpr size_t SERIAL_LENGTH = 10;

// ADV-001: the Complete Local Name, carried only in the scan response --
// which the tracker delivers as an event separate from the advertisement
// that carries the serial. A Volcano is therefore recognised by seeing both,
// under the same address.
inline constexpr const char *VOLCANO_ADVERTISED_NAME = "S&B VOLCANO H";

// How many distinct S&B devices are tracked at once. More than this in range
// of a single Dial is not a case worth RAM on a board that is already tight;
// the extras are ignored rather than displacing what was seen first.
inline constexpr size_t MAX_PAIRING_CANDIDATES = 4;

// ADV-001 records no serial format beyond "10 bytes of ASCII", so this only
// insists on printable characters -- enough to reject the unrelated bytes a
// non-Volcano S&B product's manufacturer data would carry in that position.
// `data` is what follows the company ID, as the tracker delivers it.
inline bool serial_from_manufacturer_data(const uint8_t *data, size_t len, std::string &serial) {
  if (data == nullptr || len < SERIAL_LENGTH)
    return false;
  for (size_t i = 0; i < SERIAL_LENGTH; i++) {
    if (data[i] < 0x20 || data[i] > 0x7E)
      return false;
  }
  serial.assign(reinterpret_cast<const char *>(data), SERIAL_LENGTH);
  return true;
}

// "AA:BB:CC:DD:EE:FF", upper case -- the form ESPHome itself logs a BLE
// address in.
inline std::string format_address(uint64_t address) {
  static const char HEX[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(17);
  for (int shift = 40; shift >= 0; shift -= 8) {
    uint8_t byte = static_cast<uint8_t>((address >> shift) & 0xFF);
    if (!out.empty())
      out += ':';
    out += HEX[byte >> 4];
    out += HEX[byte & 0x0F];
  }
  return out;
}

namespace detail {
inline int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
}  // namespace detail

// Strict "XX:XX:XX:XX:XX:XX", either case. All-zero is refused: 0 is what
// ble_client reads as "no address", so it can never be a pairing target.
inline bool parse_address(const std::string &text, uint64_t &address) {
  if (text.size() != 17)
    return false;
  uint64_t value = 0;
  for (size_t i = 0; i < 17; i++) {
    if (i % 3 == 2) {
      if (text[i] != ':')
        return false;
      continue;
    }
    int nibble = detail::hex_value(text[i]);
    if (nibble < 0)
      return false;
    value = (value << 4) | static_cast<uint64_t>(nibble);
  }
  if (value == 0)
    return false;
  address = value;
  return true;
}

// Decides which Volcano to pair with. Time is passed in rather than read, so
// the scan window is testable with a deterministic clock, as VolcanoDevice
// does for its own timeouts.
//
// Outcome once a scan window closes, per ADR-0013: exactly one Volcano seen
// is adopted without asking; several are held for the user to choose from;
// none is reported as not found. A Volcano is only ever counted once both
// its serial and its name have been seen under the same address.
class PairingSelector {
 public:
  enum class State {
    // A scan window is open; nothing is decided yet.
    SCANNING,
    // The window closed with several Volcanos seen; awaiting select().
    CHOOSING,
    // The window closed with none seen. Terminal until start_scan().
    NOT_FOUND,
    // An address is known and is what the Dial connects to.
    PAIRED,
  };

  void set_scan_window_ms(uint32_t ms) { this->scan_window_ms_ = ms; }

  // Boot: `stored` is the persisted address, or 0 if there is none. A stored
  // address means PAIRED straight away -- no scan, and from there the Dial
  // behaves exactly as it did with a compile-time address. Otherwise a scan
  // window opens.
  void begin(uint64_t stored, uint32_t now) {
    if (stored != 0) {
      this->pair_(stored);
    } else {
      this->open_window_(now);
    }
  }

  // Opens a fresh scan window, discarding anything seen so far. Refused
  // (false) while paired: replacing a paired device is forget()'s job, so a
  // stray rescan cannot silently drop the current one.
  bool start_scan(uint32_t now) {
    if (this->state_ == State::PAIRED)
      return false;
    this->open_window_(now);
    return true;
  }

  // The advertisement's serial, already extracted by
  // serial_from_manufacturer_data(). `rssi` is kept as the strongest seen,
  // which is what orders the choice list.
  void observe_serial(uint64_t address, const std::string &serial, int rssi) {
    Entry *entry = this->entry_for_(address);
    if (entry == nullptr)
      return;
    entry->has_serial = true;
    for (size_t i = 0; i < SERIAL_LENGTH; i++)
      entry->serial[i] = serial[i];
    entry->serial[SERIAL_LENGTH] = '\0';
    this->note_rssi_(entry, rssi);
  }

  // The scan response's local name. Anything but the Volcano's own is
  // ignored, so an unrelated device's name never occupies a slot.
  void observe_name(uint64_t address, const std::string &name, int rssi) {
    if (name != VOLCANO_ADVERTISED_NAME)
      return;
    Entry *entry = this->entry_for_(address);
    if (entry == nullptr)
      return;
    entry->has_name = true;
    this->note_rssi_(entry, rssi);
  }

  // Closes the window once it has run its length. Returns whether the state
  // changed, so the caller knows to persist and publish.
  bool process(uint32_t now) {
    if (this->state_ != State::SCANNING || now - this->window_started_ < this->scan_window_ms_)
      return false;
    this->settle_();
    switch (this->candidate_count()) {
      case 0:
        this->state_ = State::NOT_FOUND;
        break;
      case 1:
        this->pair_(this->entries_[0].address);
        break;
      default:
        this->state_ = State::CHOOSING;
        break;
    }
    return true;
  }

  // Picks the `index`th candidate, in the strongest-first order
  // candidate_*() report them in. Only meaningful while CHOOSING.
  bool select(size_t index) {
    if (this->state_ != State::CHOOSING || index >= this->candidate_count())
      return false;
    this->pair_(this->entries_[index].address);
    return true;
  }

  // Pairs with an address given directly -- the web_server / Home Assistant
  // path -- from any state, including replacing one already paired. The
  // caller has already validated it with parse_address().
  void set_address(uint64_t address) {
    if (address != 0)
      this->pair_(address);
  }

  // Clears the paired device and opens a fresh scan window, for a replaced
  // Volcano.
  void forget(uint32_t now) { this->open_window_(now); }

  State state() const { return this->state_; }
  uint64_t address() const { return this->address_; }

  // Volcanos seen with both serial and name, strongest first. Stable once the
  // window has closed; while it is still open the count can grow.
  size_t candidate_count() const {
    size_t n = 0;
    for (size_t i = 0; i < this->used_; i++) {
      if (this->entries_[i].confirmed())
        n++;
    }
    return n;
  }
  uint64_t candidate_address(size_t index) const {
    const Entry *entry = this->nth_candidate_(index);
    return entry == nullptr ? 0 : entry->address;
  }
  const char *candidate_serial(size_t index) const {
    const Entry *entry = this->nth_candidate_(index);
    return entry == nullptr ? "" : entry->serial;
  }
  int candidate_rssi(size_t index) const {
    const Entry *entry = this->nth_candidate_(index);
    return entry == nullptr ? 0 : entry->rssi;
  }

 private:
  struct Entry {
    uint64_t address{0};
    char serial[SERIAL_LENGTH + 1]{};
    int rssi{-127};
    bool has_serial{false};
    bool has_name{false};
    bool confirmed() const { return this->has_serial && this->has_name; }
  };

  void open_window_(uint32_t now) {
    this->state_ = State::SCANNING;
    this->address_ = 0;
    this->used_ = 0;
    this->window_started_ = now;
  }

  void pair_(uint64_t address) {
    this->state_ = State::PAIRED;
    this->address_ = address;
  }

  // Only tracked while a window is open: once it has closed, the candidate
  // list a chooser is showing must not shift underneath it.
  Entry *entry_for_(uint64_t address) {
    if (this->state_ != State::SCANNING)
      return nullptr;
    for (size_t i = 0; i < this->used_; i++) {
      if (this->entries_[i].address == address)
        return &this->entries_[i];
    }
    if (this->used_ >= MAX_PAIRING_CANDIDATES)
      return nullptr;
    Entry *entry = &this->entries_[this->used_++];
    *entry = Entry{};
    entry->address = address;
    return entry;
  }

  static void note_rssi_(Entry *entry, int rssi) {
    if (rssi > entry->rssi)
      entry->rssi = rssi;
  }

  // Drops entries that never became a full match and orders the rest
  // strongest first, so index 0 is the likeliest to be the unit the user is
  // standing next to.
  void settle_() {
    size_t kept = 0;
    for (size_t i = 0; i < this->used_; i++) {
      if (this->entries_[i].confirmed())
        this->entries_[kept++] = this->entries_[i];
    }
    this->used_ = kept;
    for (size_t i = 1; i < this->used_; i++) {
      Entry moving = this->entries_[i];
      size_t j = i;
      while (j > 0 && this->entries_[j - 1].rssi < moving.rssi) {
        this->entries_[j] = this->entries_[j - 1];
        j--;
      }
      this->entries_[j] = moving;
    }
  }

  const Entry *nth_candidate_(size_t index) const {
    for (size_t i = 0; i < this->used_; i++) {
      if (!this->entries_[i].confirmed())
        continue;
      if (index == 0)
        return &this->entries_[i];
      index--;
    }
    return nullptr;
  }

  State state_{State::NOT_FOUND};
  uint64_t address_{0};
  uint32_t scan_window_ms_{10000};
  uint32_t window_started_{0};
  Entry entries_[MAX_PAIRING_CANDIDATES];
  size_t used_{0};
};

}  // namespace volcano
}  // namespace esphome
