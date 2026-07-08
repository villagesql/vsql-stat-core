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
// connection thread by the POSTEXECUTE hook and shipped by a sink. The field
// set mirrors the subset of StatementEventArgs the core captures; sinks read
// whatever they need from it.
struct EventRow {
  std::string query; // truncated to statement_max_bytes
  std::string user;
  std::string client_ip;
  std::string schema;
  std::string sql_command;
  uint64_t connection_id = 0;
  bool in_transaction = false;  // was the statement inside an open transaction
  uint64_t query_start_utime = 0;
  double query_time_secs = 0.0;
  double lock_time_secs = 0.0;
  uint64_t rows_sent = 0;
  uint64_t rows_examined = 0;
  uint64_t rows_affected = 0;
  uint64_t warning_count = 0;
  int status = 0;
};

} // namespace vsql_stat

#endif // VSQL_STAT_CORE_EVENT_ROW_H
