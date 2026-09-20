#include "volcano_pairing.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace volcano {

static const char *const TAG = "volcano.pairing";

namespace {

// A BLE address is 48 bits; anything wider in storage is not one, and is
// treated as nothing stored rather than handed to the ble_client.
constexpr uint64_t ADDRESS_MASK = 0xFFFFFFFFFFFFULL;

}  // namespace

void VolcanoPairing::setup() {
  this->preference_ = global_preferences->make_preference<uint64_t>(fnv1_hash("volcano_pairing_address"));
  uint64_t stored = 0;
  if (!this->preference_.load(&stored) || (stored & ~ADDRESS_MASK) != 0)
    stored = 0;
  this->saved_address_ = stored;

  this->selector_.begin(stored, millis());

  // Unconditional, including to 0: the ble_client's own `mac_address` is a
  // compile-time placeholder once pairing is configured, and this is what
  // decides its real target. With no address it never matches an
  // advertisement, so it does not connect.
  if (this->ble_client_ != nullptr)
    this->ble_client_->set_address(stored);

  if (stored != 0) {
    ESP_LOGI(TAG, "Paired Volcano %s restored from storage", format_address(stored).c_str());
  } else {
    ESP_LOGI(TAG, "No Volcano paired; scanning for one");
  }

  this->set_interval("volcano_pairing_update", 500, [this] { this->update_(); });
}

void VolcanoPairing::dump_config() {
  ESP_LOGCONFIG(TAG, "Volcano pairing:");
  ESP_LOGCONFIG(TAG, "  Status: %s", this->status_text().c_str());
  ESP_LOGCONFIG(TAG, "  Recognises a Volcano Hybrid by its serial-bearing advertisement plus the '%s' scan response",
                VOLCANO_ADVERTISED_NAME);
}

// ADV-001: the serial is in the advertisement's manufacturer data and the
// name is in the scan response, which arrive as separate events -- so each is
// reported on its own and PairingSelector matches them up by address.
// Always false: this listener only watches, and never claims an
// advertisement for itself.
bool VolcanoPairing::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (this->selector_.state() != State::SCANNING)
    return false;

  const uint64_t address = device.address_uint64();
  const int rssi = device.get_rssi();

  const auto company_id = esp32_ble_tracker::ESPBTUUID::from_uint16(STORZ_BICKEL_COMPANY_ID);
  for (const auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (!(manufacturer_data.uuid == company_id))
      continue;
    std::string serial;
    if (serial_from_manufacturer_data(manufacturer_data.data.data(), manufacturer_data.data.size(), serial))
      this->selector_.observe_serial(address, serial, rssi);
  }

  if (!device.get_name().empty())
    this->selector_.observe_name(address, device.get_name(), rssi);
  return false;
}

bool VolcanoPairing::rescan() {
  bool started = this->selector_.start_scan(millis());
  if (started) {
    ESP_LOGI(TAG, "Scanning for a Volcano");
  } else {
    ESP_LOGW(TAG, "Rescan refused: already paired; forget the current Volcano first");
  }
  this->sync_();
  return started;
}

bool VolcanoPairing::select(size_t index) {
  bool selected = this->selector_.select(index);
  if (!selected)
    ESP_LOGW(TAG, "Ignoring selection %u: nothing to choose at that position", static_cast<unsigned>(index));
  this->sync_();
  return selected;
}

void VolcanoPairing::forget() {
  ESP_LOGI(TAG, "Forgetting the paired Volcano; scanning for one");
  this->selector_.forget(millis());
  this->sync_();
}

bool VolcanoPairing::set_address_from_text(const std::string &text) {
  if (text.empty()) {
    this->forget();
    return true;
  }
  uint64_t address = 0;
  if (!parse_address(text, address)) {
    ESP_LOGW(TAG, "Ignoring '%s': not an address of the form AA:BB:CC:DD:EE:FF", text.c_str());
    // Publish again so an entity that has just been typed into shows what is
    // really paired, not the rejected text.
    this->published_ = false;
    this->sync_();
    return false;
  }
  this->selector_.set_address(address);
  this->sync_();
  return true;
}

std::string VolcanoPairing::address_str() const {
  return this->selector_.address() == 0 ? "" : format_address(this->selector_.address());
}

std::string VolcanoPairing::candidate_address_str(size_t index) const {
  uint64_t address = this->selector_.candidate_address(index);
  return address == 0 ? "" : format_address(address);
}

// Kept short: it is shown on a web_server page and as one row of the Dial's
// own Pairing page, and a Home Assistant text sensor reading tops out well
// short of what a longer sentence would need.
std::string VolcanoPairing::status_text() const {
  switch (this->selector_.state()) {
    case State::SCANNING:
      return "Searching";
    case State::CHOOSING: {
      std::string text = "Choose one:";
      for (size_t i = 0; i < this->selector_.candidate_count(); i++) {
        text += i == 0 ? " " : ", ";
        text += this->selector_.candidate_serial(i);
      }
      return text;
    }
    case State::NOT_FOUND:
      // CONN-003: a Volcano held by another client -- the official app,
      // say -- stops advertising, and looks exactly like one that is off.
      return "Not found";
    case State::PAIRED:
      return "Paired";
  }
  return "";
}

void VolcanoPairing::update_() {
  this->selector_.process(millis());
  this->sync_();
}

void VolcanoPairing::sync_() {
  const uint64_t address = this->selector_.address();
  if (address != this->saved_address_) {
    this->saved_address_ = address;
    this->preference_.save(&address);
    // Pairing changes are rare, and losing one to an untimely power cut would
    // silently undo a choice the user just made -- so flush now rather than
    // wait for the periodic sync.
    global_preferences->sync();
    if (address != 0) {
      ESP_LOGI(TAG, "Paired with Volcano %s", format_address(address).c_str());
    } else {
      ESP_LOGI(TAG, "Cleared the paired Volcano");
    }
  }
  this->apply_address_();
  this->publish_();
}

// Brings the ble_client's target in line with the selector's. Changing the
// address of a client that is mid-connection would leave its CLOSE_EVT
// unmatched, so a live or pending connection to the wrong unit is first
// disconnected and the address changed once the client is idle -- a few
// passes later, at most.
void VolcanoPairing::apply_address_() {
  if (this->ble_client_ == nullptr)
    return;
  const uint64_t wanted = this->selector_.address();
  if (this->ble_client_->get_address() == wanted)
    return;

  const auto state = this->ble_client_->state();
  if (state == esp32_ble_tracker::ClientState::INIT || state == esp32_ble_tracker::ClientState::IDLE) {
    this->ble_client_->set_address(wanted);
    return;
  }
  if (state != esp32_ble_tracker::ClientState::DISCONNECTING)
    this->ble_client_->disconnect();
}

void VolcanoPairing::publish_() {
  const std::string address = this->address_str();
  const std::string status = this->status_text();

  if (this->address_text_ != nullptr && (!this->published_ || address != this->published_address_))
    this->address_text_->publish_state(address);
  if (this->status_text_sensor_ != nullptr && (!this->published_ || status != this->published_status_))
    this->status_text_sensor_->publish_state(status);

  this->published_address_ = address;
  this->published_status_ = status;
  this->published_ = true;
}

void VolcanoAddressText::control(const std::string &value) { this->parent_->set_address_from_text(value); }

}  // namespace volcano
}  // namespace esphome

#endif  // USE_ESP32
