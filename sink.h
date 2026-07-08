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

#ifndef VSQL_STAT_CORE_SINK_H
#define VSQL_STAT_CORE_SINK_H

#include <string>
#include <vector>

#include "event_row.h"

namespace vsql_stat {

// A destination for captured events. The core owns capture/queue/worker and
// drives a single registered Sink; each backend (HTTP, ClickHouse-native,
// slow-log, ...) is one Sink implementation. Everything transport- and
// format-specific lives inside the sink.
//
// Threading: flush() runs only on the core's single background worker thread,
// off the query path, never concurrently for a given sink instance. accept()
// runs on the connection thread inside the hook, so it must be cheap and
// thread-safe (read-only over the row + the sink's own config).
class Sink {
public:
  virtual ~Sink() = default;

  // Per-sink capture filter. Return false to skip an event before it is queued
  // (e.g. the slow-log sink accepts only rows over its threshold). Default:
  // accept everything (e.g. ClickHouse wants raw events, aggregate downstream).
  virtual bool accept(const EventRow & /*row*/) const { return true; }

  // Ship a batch of events. Return true on success; on failure return false and
  // set `err` (for the error log / a flush-error counter). The batch is the
  // sink's to serialize however its backend wants. v1 has no retry buffer: a
  // failed batch is dropped by the caller and only later-queued events are
  // retried on the next flush.
  virtual bool flush(const std::vector<EventRow> &batch, std::string &err) = 0;
};

} // namespace vsql_stat

#endif // VSQL_STAT_CORE_SINK_H
