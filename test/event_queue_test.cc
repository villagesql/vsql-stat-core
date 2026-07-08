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

// Standalone unit test for the bounded EventQueue. No test framework and no
// VillageSQL SDK -- pure C++ over event_queue + event_row, so core's queue
// logic (capacity, drop-newest overflow, drain semantics, counters) can be
// exercised in isolation, without a sink, a server, or ClickHouse.

#include "event_queue.h"

#include <cstdio>
#include <string>
#include <vector>

using vsql_stat::EventQueue;
using vsql_stat::EventRow;

static int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

// Build an EventRow tagged with a marker so FIFO order can be asserted.
static EventRow row(const std::string &q) {
  EventRow r;
  r.query = q;
  return r;
}

static void test_push_until_full_then_drop_newest() {
  EventQueue q(3);
  CHECK(q.capacity() == 3);
  CHECK(q.depth() == 0);
  CHECK(q.dropped() == 0);

  CHECK(q.push(row("a")) == true);
  CHECK(q.push(row("b")) == true);
  CHECK(q.push(row("c")) == true);
  CHECK(q.depth() == 3);

  // Full: the NEWEST (incoming) event is dropped, not an existing one.
  CHECK(q.push(row("d")) == false);
  CHECK(q.push(row("e")) == false);
  CHECK(q.depth() == 3);   // still the first three
  CHECK(q.dropped() == 2); // two drops counted
}

static void test_drain_fifo_and_partial() {
  EventQueue q(10);
  for (int i = 0; i < 5; ++i)
    CHECK(q.push(row(std::to_string(i))));
  CHECK(q.depth() == 5);

  std::vector<EventRow> out;
  // Drain fewer than present: get exactly `max`, in FIFO order.
  CHECK(q.drain(out, 2) == 2);
  CHECK(out.size() == 2);
  CHECK(out[0].query == "0");
  CHECK(out[1].query == "1");
  CHECK(q.depth() == 3);

  // Drain more than present: clamped to what's left, appended to `out`.
  CHECK(q.drain(out, 100) == 3);
  CHECK(out.size() == 5);
  CHECK(out[2].query == "2");
  CHECK(out[4].query == "4");
  CHECK(q.depth() == 0);
}

static void test_drain_empty() {
  EventQueue q(4);
  std::vector<EventRow> out;
  CHECK(q.drain(out, 10) == 0);
  CHECK(out.empty());
  CHECK(q.depth() == 0);
}

static void test_drain_then_refill() {
  EventQueue q(2);
  CHECK(q.push(row("x")));
  CHECK(q.push(row("y")));
  CHECK(q.push(row("z")) == false); // full -> drop
  CHECK(q.dropped() == 1);

  std::vector<EventRow> out;
  CHECK(q.drain(out, 2) == 2); // empties the queue
  CHECK(q.depth() == 0);

  // Space freed: new pushes succeed again; dropped() is cumulative (not reset).
  CHECK(q.push(row("w")) == true);
  CHECK(q.depth() == 1);
  CHECK(q.dropped() == 1);
}

static void test_zero_capacity_drops_all() {
  EventQueue q(0);
  CHECK(q.push(row("a")) == false);
  CHECK(q.depth() == 0);
  CHECK(q.dropped() == 1);
}

int main() {
  test_push_until_full_then_drop_newest();
  test_drain_fifo_and_partial();
  test_drain_empty();
  test_drain_then_refill();
  test_zero_capacity_drops_all();

  if (g_failures == 0) {
    std::printf("event_queue_test: all checks passed\n");
    return 0;
  }
  std::printf("event_queue_test: %d check(s) FAILED\n", g_failures);
  return 1;
}
