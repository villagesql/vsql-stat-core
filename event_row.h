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

#ifndef VSQL_STAT_CORE_EVENT_ROW_H
#define VSQL_STAT_CORE_EVENT_ROW_H

#include <cstdint>
#include <string>

namespace vsql_stat {

// One captured statement event -- a plain, backend-agnostic row built on the
// connection thread by the POSTEXECUTE hook and shipped by a sink. It captures
// every field StatementEventArgs exposes; sinks read whatever they need from
// it. Capturing everything keeps all downstream options open (a field dropped
// at capture can never be filtered, grouped, or aggregated later); trimming the
// volume is a later optimization (a filter or a capture toggle), not a reason
// to discard fields here.
struct EventRow {
  std::string query; // truncated to statement_max_bytes
  std::string user;
  std::string client_ip;
  std::string schema;
  std::string sql_command;
  uint64_t connection_id = 0;
  uint16_t port = 0;           // client port
  bool in_transaction = false; // was the statement inside an open transaction

  // Outcome.
  int status = 0;            // 0 on success, else the MySQL error code
  std::string sqlstate;      // 5-char SQLSTATE, empty on success
  std::string error_message; // error text, empty on success
  uint64_t warning_count = 0;

  // Timing.
  uint64_t query_start_utime = 0; // microseconds since epoch
  double query_time_secs = 0.0;
  double lock_time_secs = 0.0;

  // Rows and bytes.
  uint64_t rows_sent = 0;
  uint64_t rows_examined = 0;
  uint64_t rows_affected = 0;
  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;

  // Normalized (digested) query -- literals stripped, for grouping similar
  // queries regardless of literal values. Empty if digest was disabled or the
  // query was too long to digest.
  std::string digest_text;

  // Digest hash: the statement identity as a 64-char hex string (the same value
  // performance_schema exposes as DIGEST). Compact GROUP BY key; empty when no
  // digest is available.
  std::string digest_hash;

  // Optimizer quality indicators (non-zero suggests inefficient execution).
  uint64_t select_full_join = 0;       // joins without usable index
  uint64_t select_full_range_join = 0; // joins using range on ref table
  uint64_t select_range = 0;           // range scans on first table
  uint64_t select_range_check = 0;     // joins with key check per row
  uint64_t select_scan = 0;            // full scans of first table

  // Sort metrics.
  uint64_t sort_merge_passes = 0; // merge passes (high = large sort)
  uint64_t sort_range = 0;        // sorts using a range
  uint64_t sort_rows = 0;         // rows sorted
  uint64_t sort_scan = 0;         // sorts using a full table scan

  // Temporary table usage.
  uint64_t created_tmp_tables = 0;      // tmp tables created (memory or disk)
  uint64_t created_tmp_disk_tables = 0; // tmp tables spilled to disk

  // Index usage.
  bool no_index_used = false;      // ran without a usable index
  bool no_good_index_used = false; // no good index was found

  // Handler row-access counters (the slow log's Read_* fields) -- how rows were
  // accessed at the storage-engine level. read_rnd_next high = full table scan;
  // read_key/read_next = index access.
  uint64_t read_first = 0;
  uint64_t read_last = 0;
  uint64_t read_key = 0;
  uint64_t read_next = 0;
  uint64_t read_prev = 0;
  uint64_t read_rnd = 0;
  uint64_t read_rnd_next = 0;
};

} // namespace vsql_stat

#endif // VSQL_STAT_CORE_EVENT_ROW_H
