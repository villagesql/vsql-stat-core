// Copyright (c) 2026 VillageSQL Contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <https://www.gnu.org/licenses/>.

#ifndef VSQL_STAT_CORE_EVENT_QUEUE_H
#define VSQL_STAT_CORE_EVENT_QUEUE_H

#include <cstdint>
#include <mutex>
#include <vector>

#include "event_row.h"

namespace vsql_stat {

// Bounded producer/consumer queue between the (many) connection-thread hook
// producers and the single flush-worker consumer. Thread-safe; the critical
// section is only the push / drain-batch, never the sink send.
//
// Overflow policy: drop-NEWEST when full (push returns false and increments the
// caller-visible drop count via dropped()). Never blocks -- blocking would push
// sink latency back onto the query path.
class EventQueue {
public:
  explicit EventQueue(size_t capacity) : capacity_(capacity) {}

  // Enqueue one event. Returns true if queued, false if dropped (queue full).
  // Called on connection threads; must stay cheap.
  bool push(EventRow &&row);

  // Move up to `max` events out of the queue into `out` (appended). Returns the
  // number drained. Called by the flush worker.
  size_t drain(std::vector<EventRow> &out, size_t max);

  size_t depth() const;
  size_t capacity() const { return capacity_; }
  uint64_t dropped() const;

private:
  mutable std::mutex mu_;
  std::vector<EventRow> buf_; // guarded by mu_
  size_t capacity_;
  uint64_t dropped_ = 0; // guarded by mu_
};

} // namespace vsql_stat

#endif // VSQL_STAT_CORE_EVENT_QUEUE_H
