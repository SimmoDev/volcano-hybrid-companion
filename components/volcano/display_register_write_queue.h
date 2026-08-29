#pragma once

#include "volcano_ble_client_observer.h"

#include "esphome/core/optional.h"

#include <deque>

namespace esphome {
namespace volcano {

// CHAR-009 carries two independent settings (display-on-cooling, CMD-005;
// display units, CMD-010) behind one handle, written with two distinct
// mask-and-action payloads. write_display_on_cooling() and
// write_display_units_fahrenheit() can each be called independently of the
// other -- e.g. two entities toggled in quick succession -- so a write to
// one can be issued before the other's ESP_GATTC_WRITE_CHAR_EVT arrives.
// Tracking "the most recent write" in one field would attribute that event
// to whichever call happened to run last, not the write it actually
// completes, so this is a FIFO of every write issued on this handle but not
// yet completed instead: each write pushes its own field once
// esp_ble_gattc_write_char() itself reports success, and each completion
// pops the front to find out which field it belongs to. Order is preserved
// because ATT write requests on one handle complete in the order they were
// sent.
//
// Every push is matched by exactly one ESP_GATTC_WRITE_CHAR_EVT: these
// writes use Write With Response, for which ESP-IDF always delivers a
// completion (success or error) unless the link drops first -- and a drop
// calls clear(). Nothing else empties the queue; in particular
// VolcanoDevice's 5s pending-write timeout does not reach it, so that
// Write-With-Response guarantee is what keeps a later completion from
// popping a stale entry.
//
// No BLE/ESP-IDF dependency, unlike VolcanoBleClient itself, so the FIFO
// ordering this exists to guarantee is host-testable directly -- see
// components/volcano/test/display_register_write_queue_test.cpp.
class DisplayRegisterWriteQueue {
 public:
  void push(VolcanoField field) { this->queue_.push_back(field); }

  // Which field the next completion on this handle belongs to, popping it
  // off the front -- or nullopt if a completion arrived with nothing
  // tracked, which never happens in ordinary operation (every completion
  // has a matching push) but is handled rather than assumed away.
  optional<VolcanoField> pop() {
    if (this->queue_.empty())
      return nullopt;
    VolcanoField field = this->queue_.front();
    this->queue_.pop_front();
    return field;
  }

  // Every write queued here will never get its ESP_GATTC_WRITE_CHAR_EVT
  // once the link is gone; called on disconnect so a future connection's
  // first completion on this handle pops a fresh entry, never a stale one.
  void clear() { this->queue_.clear(); }

 private:
  std::deque<VolcanoField> queue_;
};

}  // namespace volcano
}  // namespace esphome
