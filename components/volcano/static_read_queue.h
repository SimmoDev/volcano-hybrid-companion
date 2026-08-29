#pragma once

#include <cstdint>

namespace esphome {
namespace volcano {

// The ordered set of characteristics VolcanoBleClient reads once per
// connection rather than subscribing to -- the auto-shutoff duration, LED
// brightness, and the five device-information strings (see
// VolcanoBleClient::queue_static_reads_()). Only one read is ever
// outstanding: issue_next_static_read_() starts the next from the previous
// one's completion, and its completion is what on_ready() waits for.
//
// This class owns only the bookkeeping -- which handle is due next, and
// whether an incoming ESP_GATTC_READ_CHAR_EVT is the sweep's own -- with no
// BLE or ESP-IDF dependency, unlike the rest of VolcanoBleClient. The
// esp_ble_gattc_read_char() calls stay in VolcanoBleClient; the ordering
// guarantee this exists for is host-tested directly (see
// components/volcano/test/static_read_queue_test.cpp), the same split
// display_register_write_queue.h already makes.
//
// advance_if_current() is the only thing that attributes a read completion
// to the sweep: ESP_GATTC_READ_CHAR_EVT carries no request identity, so a
// handle-and-position match against the current slot is all there is to go
// on. A write's own read-back on a duration/LED-brightness handle cannot be
// confused with the sweep's read of the same handle, because that write is
// refused until the sweep is done (VolcanoBleClient::static_sweep_done_), by
// which point done() is already true and advance_if_current() returns false.
class StaticReadQueue {
 public:
  // The number of handles queue_static_reads_() enqueues. A static_assert
  // there keeps this and that list in step; enqueue() also guards against
  // it, so an over-long list drops entries rather than overrunning.
  static const uint8_t CAPACITY = 7;

  // Discards any partial sweep. Called at the start of each connection's
  // service discovery and on disconnect.
  void reset() {
    this->count_ = 0;
    this->index_ = 0;
  }

  // Appends a handle to read. A zero handle -- a characteristic not
  // resolved on this device -- is skipped rather than queued.
  void enqueue(uint16_t handle) {
    if (handle != 0 && this->count_ < CAPACITY)
      this->handles_[this->count_++] = handle;
  }

  // Whether every queued read has been accounted for. True on an empty
  // queue, and the point on_ready() fires.
  bool done() const { return this->index_ >= this->count_; }

  // The handle whose read should be issued next, or 0 once done().
  uint16_t current() const { return this->index_ < this->count_ ? this->handles_[this->index_] : 0; }

  // Advances past the current handle unconditionally -- for a read that
  // could not even be issued, so one unreadable characteristic does not
  // strand the rest.
  void skip() {
    if (this->index_ < this->count_)
      this->index_++;
  }

  // Whether `handle` is the read the sweep is currently waiting on, and if
  // so, advances past it. Returns false once the sweep is done, so a later
  // read of the same handle (a write's own read-back) is not mistaken for
  // the sweep's.
  bool advance_if_current(uint16_t handle) {
    if (this->index_ < this->count_ && this->handles_[this->index_] == handle) {
      this->index_++;
      return true;
    }
    return false;
  }

 private:
  // Value-initialised even though every read is already gated by
  // index_ < count_ and a slot is always written by enqueue() before
  // count_ admits it -- so the zeroing is belt-and-braces, not relied on.
  uint16_t handles_[CAPACITY]{};
  uint8_t count_{0};
  uint8_t index_{0};
};

}  // namespace volcano
}  // namespace esphome
