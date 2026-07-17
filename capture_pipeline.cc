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

#include "capture_pipeline.h"

#include <string>
#include <vector>

#include "event_queue.h"
#include "event_row.h"

namespace vsql_stat {

namespace {

// Status counters are long long (the sys-var/status-var capabilities bind them
// via long long*; see capture_pipeline.h). Concurrent access -- hook on many
// connection threads, worker on its own -- uses relaxed atomic ops on the
// storage in place. (std::atomic_ref is the C++20 way; these __atomic builtins
// are the C++17-safe equivalent on GCC/Clang.)
inline void atomic_add(long long *v, long long n) {
  __atomic_fetch_add(v, n, __ATOMIC_RELAXED);
}
inline void atomic_store(long long *v, long long n) {
  __atomic_store_n(v, n, __ATOMIC_RELAXED);
}

Sink *g_sink = nullptr;
Config g_config{};
Status g_status{};

// Whether capture is active. The thread_worker owns the `enabled` sysvar; the
// flush handler flips this on ENABLE/DISABLE so the hook (which fires on
// connection threads, independent of the worker) can cheaply skip capturing
// when disabled rather than enqueue into a queue nobody is draining.
std::atomic<bool> g_capturing{false};

// The queue, built lazily from queue_capacity at first use (capacity changes
// take effect on reload -- resizing a live bounded queue is a later
// refinement).
EventQueue &queue() {
  const int64_t cap = (g_config.queue_capacity && *g_config.queue_capacity > 0)
                          ? *g_config.queue_capacity
                          : 100000;
  static EventQueue q(static_cast<size_t>(cap));
  return q;
}

std::string arg_str(const char *v) {
  return v != nullptr ? std::string(v) : "";
}

} // namespace

void init(Sink *sink, const Config &config, const Status &status) {
  g_sink = sink;
  g_config = config;
  g_status = status;
}

void on_statement(
    const ::vsql::preview_statement_event::StatementEventArgs &args) {
  if (!g_capturing.load(std::memory_order_relaxed))
    return;
  if (g_sink == nullptr)
    return;

  EventRow row;
  const std::string_view q = args.query();
  const size_t max_bytes = static_cast<size_t>(
      (g_config.statement_max_bytes && *g_config.statement_max_bytes > 0)
          ? *g_config.statement_max_bytes
          : 0);
  row.query.assign(q.data(),
                   max_bytes && q.size() > max_bytes ? max_bytes : q.size());
  row.user = arg_str(args.user());
  row.client_ip = arg_str(args.client_ip());
  row.schema = arg_str(args.schema());
  row.sql_command = arg_str(args.sql_command());
  row.connection_id = args.connection_id();
  row.port = args.port();
  row.in_transaction = args.in_transaction();
  row.status = args.status();
  row.sqlstate = arg_str(args.sqlstate());
  row.error_message = arg_str(args.error_message());
  row.warning_count = args.warning_count();
  row.query_start_utime = args.query_start_utime();
  row.query_time_secs = args.query_time_secs();
  row.lock_time_secs = args.lock_time_secs();
  row.rows_sent = args.rows_sent();
  row.rows_examined = args.rows_examined();
  row.rows_affected = args.rows_affected();
  row.bytes_sent = args.bytes_sent();
  row.bytes_received = args.bytes_received();
  row.digest_text = arg_str(args.digest_text());
  row.select_full_join = args.select_full_join();
  row.select_full_range_join = args.select_full_range_join();
  row.select_range = args.select_range();
  row.select_range_check = args.select_range_check();
  row.select_scan = args.select_scan();
  row.sort_merge_passes = args.sort_merge_passes();
  row.sort_range = args.sort_range();
  row.sort_rows = args.sort_rows();
  row.sort_scan = args.sort_scan();
  row.created_tmp_tables = args.created_tmp_tables();
  row.created_tmp_disk_tables = args.created_tmp_disk_tables();
  row.no_index_used = args.no_index_used();
  row.no_good_index_used = args.no_good_index_used();

  // Per-sink filter before we spend a queue slot (e.g. slow-log threshold).
  if (!g_sink->accept(row))
    return;

  atomic_add(g_status.events_captured, 1);
  queue().push(std::move(row)); // drop-newest on full; counted in the queue
  atomic_store(g_status.events_dropped,
               static_cast<int64_t>(queue().dropped()));
}

vef_next_wakeup_t on_flush(vef_wakeup_reason_t reason) {
  const unsigned int interval = static_cast<unsigned int>(
      (g_config.flush_interval_ms && *g_config.flush_interval_ms > 0)
          ? *g_config.flush_interval_ms
          : 1000);

  if (reason == VEF_WAKEUP_ENABLE) {
    g_capturing.store(true, std::memory_order_relaxed); // hook starts capturing
    return {interval, 0};                               // start periodic timer
  }
  if (reason == VEF_WAKEUP_DISABLE) {
    g_capturing.store(false, std::memory_order_relaxed);
    return {};
  }

  const size_t batch_max = static_cast<size_t>(
      (g_config.batch_max && *g_config.batch_max > 0) ? *g_config.batch_max
                                                      : 1000);
  std::vector<EventRow> batch;
  queue().drain(batch, batch_max);
  atomic_store(g_status.queue_depth, static_cast<int64_t>(queue().depth()));

  if (!batch.empty() && g_sink != nullptr) {
    std::string err;
    if (g_sink->flush(batch, err)) {
      atomic_add(g_status.events_archived, static_cast<int64_t>(batch.size()));
      // last_flush_utime: reuse the newest event's start time as a cheap,
      // clock-free stamp (wall-clock calls are avoided by convention; the event
      // carries a server-provided time).
      atomic_store(g_status.last_flush_utime,
                   static_cast<int64_t>(batch.back().query_start_utime));
    } else {
      // Failed batch is dropped (v1 -- no retry buffer); events still queued
      // are retried next wake. Counted so the failure is visible.
      atomic_add(g_status.flush_errors, 1);
    }
  }

  // Near-full early flush: if the queue is still >= ~80% capacity, come back
  // quickly instead of waiting the full interval, to stay ahead of overflow.
  const size_t depth = queue().depth();
  const size_t cap = queue().capacity();
  if (cap > 0 && depth * 5 >= cap * 4) {
    return {10, 0}; // wake again in 10ms to keep draining
  }
  return {interval, 0};
}

} // namespace vsql_stat
