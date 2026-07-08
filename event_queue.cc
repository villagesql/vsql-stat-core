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

#include "event_queue.h"

#include <algorithm>
#include <iterator>

namespace vsql_stat {

bool EventQueue::push(EventRow &&row) {
  std::lock_guard<std::mutex> lock(mu_);
  if (buf_.size() >= capacity_) {
    // Drop-newest: the incoming event is discarded. Counted so the drop is
    // visible via a status var rather than silent.
    ++dropped_;
    return false;
  }
  buf_.push_back(std::move(row));
  return true;
}

size_t EventQueue::drain(std::vector<EventRow> &out, size_t max) {
  std::lock_guard<std::mutex> lock(mu_);
  const size_t n = std::min(max, buf_.size());
  if (n == 0)
    return 0;
  out.insert(out.end(), std::make_move_iterator(buf_.begin()),
             std::make_move_iterator(buf_.begin() + n));
  buf_.erase(buf_.begin(), buf_.begin() + n);
  return n;
}

size_t EventQueue::depth() const {
  std::lock_guard<std::mutex> lock(mu_);
  return buf_.size();
}

uint64_t EventQueue::dropped() const {
  std::lock_guard<std::mutex> lock(mu_);
  return dropped_;
}

} // namespace vsql_stat
