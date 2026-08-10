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

#ifndef VSQL_STAT_CORE_CAPTURE_PIPELINE_H
#define VSQL_STAT_CORE_CAPTURE_PIPELINE_H

#include <atomic>
#include <cstdint>

#include <villagesql/abi/preview/thread_worker.h>
#include <villagesql/preview/statement_event.h>

#include "sink.h"

// Backend-agnostic capture pipeline: the reusable LOGIC shared by every
// vsql-stat sink (bounded queue + POSTEXECUTE capture + batched background
// flush + overflow/status accounting). This header intentionally exposes plain
// functions and data, NOT SDK capability objects: the SDK capabilities
// (StatementEventCapability<Phase, Fn>, ThreadWorkerCapability<Fn>,
// SysVarCapability<N>, StatusVarCapability<N>) are compile-time-templated on
// their handler/descriptors, so they must be declared in each sink's own
// extension.cc. That .cc wires two thin handlers to the two functions below and
// composes VEF_GENERATE_ENTRY_POINTS.
//
// See VSQL_STAT_ARCHITECTURE.md.

namespace vsql_stat {

// Behavior tunables (backed by the core sysvars each sink declares). Read fresh
// on each hook/flush call so live SET GLOBAL takes effect. queue_capacity is
// applied once when the queue is first created.
//
// Lifetime: these are BORROWED pointers into the sink extension's sysvar-backed
// globals (file statics, which outlive the extension). init() copies the struct
// (the pointers), not the pointees; the caller must keep the pointees alive for
// the extension's lifetime.
struct Config {
  long long *queue_capacity;      // events (queue built lazily from this)
  long long *batch_max;           // flush at this many queued events
  long long *flush_interval_ms;   // flush at least this often
  long long *statement_max_bytes; // truncate captured query text
};

// Status counters (backed by the core status vars each sink declares). int64_t
// because the status-var capability reads them via int64_t*; concurrent access
// uses relaxed atomic ops (see the .cc). Written by the hook (many connection
// threads) and the worker; read by SHOW STATUS.
//
// Lifetime: borrowed pointers into the sink's sysvar-backed globals, same
// contract as Config -- must outlive the extension.
struct Status {
  long long *events_captured;
  long long *events_archived;
  long long *events_dropped;
  long long *queue_depth;
  long long *flush_errors;
  long long *last_flush_utime;
};

// Set the active sink + the config/status bindings. Called once at load from
// the sink's extension.cc before the worker is enabled. Pointers must outlive
// the extension (file-static instances are the norm).
void init(Sink *sink, const Config &config, const Status &status);

// Capture handler: call from the sink's POSTEXECUTE StatementEventCapability
// handler. Applies the sink's accept() filter, builds an EventRow, enqueues it
// (drop-newest on overflow). Cheap and non-blocking -- no I/O. No-op until the
// worker has enabled capture.
void on_statement(
    const ::vsql::preview_statement_event::StatementEventArgs &args);

// Flush handler: call from the sink's ThreadWorkerCapability handler and return
// its result. Handles ENABLE/DISABLE (toggles capture), drains a batch, calls
// Sink::flush off the query path, updates status, and schedules the next wakeup
// (periodic, with a near-full early re-wake). vef_next_wakeup_t / the reason
// enum come from the thread_worker ABI.
vef_next_wakeup_t on_flush(vef_wakeup_reason_t reason);

} // namespace vsql_stat

#endif // VSQL_STAT_CORE_CAPTURE_PIPELINE_H
