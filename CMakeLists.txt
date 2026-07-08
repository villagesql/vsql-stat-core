# Copyright (c) 2026 VillageSQL Contributors
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses/>.

# vsql-stat-core is source-only: the sink extensions compile these files
# directly (vendored under core/ via git subtree). This CMakeLists exists ONLY
# to build and run core's standalone unit tests -- it is not used by the sink
# builds. The event_queue test is pure C++ (no VillageSQL SDK), so core's
# trickiest logic (bounded queue, drop-newest overflow, drain, counters) can be
# tested in isolation.
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build

cmake_minimum_required(VERSION 3.16)
project(vsql_stat_core_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(event_queue_test
    test/event_queue_test.cc
    event_queue.cc
)
target_include_directories(event_queue_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME event_queue_test COMMAND event_queue_test)
