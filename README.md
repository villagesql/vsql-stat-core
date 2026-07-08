# vsql-stat-core

Shared capture pipeline for the `vsql-stat` family of VillageSQL query-telemetry
extensions. This is **source-only** — the sinks compile these files directly.
Each sink extension (`vsql-stat-ch`, `vsql-stat-http`, `vsql-stat-slowlog`)
vendors these files under its own `core/` directory (via `git subtree`) and
compiles them into its `.veb`. The only build here is the standalone unit test
(below).

## What's here

Sources live at the repo root (so a sink's `git subtree add --prefix=core`
places them at `core/*` in the consumer):

| File | Role |
|------|------|
| `event_row.h` | the backend-agnostic captured statement row |
| `event_queue.{h,cc}` | bounded MPSC queue (drop-newest overflow) |
| `sink.h` | the `Sink` interface each extension implements |
| `capture_pipeline.{h,cc}` | POSTEXECUTE-hook capture + background-worker flush |
| `test/event_queue_test.cc` | standalone unit test for the queue |

A sink extension provides a `Sink` implementation and its transport/config; the
core owns capture, buffering, and the flush worker, so that logic is written
once and shared.

## Testing

The queue logic (bounded capacity, drop-newest overflow, drain, counters) has a
standalone unit test — pure C++, no VillageSQL SDK, no server, no external
service:

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

The capture pipeline (`capture_pipeline`) binds to SDK preview capabilities and
is exercised end to end via each sink's mtr suite rather than here.

## How a sink consumes it

Vendored into the extension repo under `core/` with `git subtree`:

```bash
# first time, in the extension repo:
git subtree add --prefix=core <vsql-stat-core-remote> main --squash

# to pull a later core change:
git subtree pull --prefix=core <vsql-stat-core-remote> main --squash
```

The extension's `CMakeLists.txt` then lists `core/event_queue.cc` and
`core/capture_pipeline.cc` in its `add_library` and adds the repo root to the
include path so `#include "core/..."` resolves.

## Compatibility

The core binds to the VillageSQL SDK preview capabilities (statement_event,
thread_worker) and uses fixed-width ABI types (`int64_t` for sysvar/status
bindings). It tracks the SDK the sink extensions build against.
