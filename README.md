# SdrTaskApi

Shared C++ library defining the AMQP message schema, type system, and JSON codec for the SDR Radio Resource Task Manager.

## Contents

| Header | Purpose |
|--------|---------|
| `include/sdr/Types.hpp` | All structs, enums, and helper functions — `TaskRequest`, `TaskRecord`, `TaskResponse`, `RejectCode`, `TaskState`, rank helpers, etc. |
| `include/sdr/MessageCodec.hpp` | Stateless JSON encoder/decoder for every `msg_type` |
| `src/MessageCodec.cpp` | Implementation |

## Key types

### `TaskRequest`

Decoded from incoming AMQP JSON. All task-creation messages **must** include a `rank` field — the decoder returns `nullopt` if it is absent.

| Field | Type | Description |
|-------|------|-------------|
| `msg_type` | string | `TASK_REQUEST_*`, `TASK_STOP`, `TASK_CANCEL`, `HEALTH_QUERY` |
| `request_id` | string | Client-generated UUID v4 (required) |
| `task_type` | `TaskType` | `DF`, `NARROWBAND`, `WIDEBAND`, `SCAN`, `SNAPSHOT`, `TRIGGERED`, `CALIBRATION` |
| `schedule_mode` | `ScheduleMode` | `SCHEDULED`, `IMMEDIATE`, `CONTINUOUS` |
| `priority` | int | Advisory scheduling weight |
| `rank` | int | **Required.** Preemption tier (0 = lowest). Higher rank preempts lower when spectrum is needed. |
| `rf` | `RfRequest` | Center freq, BW, SR, channel counts, preferred device, coherency group |

### `TaskRecord`

Internal task state tracked by `ResourceManager`.  The `rank` field is propagated from the originating `TaskRequest`.

### `PREEMPT_TERMINAL_REASON`

When a running task is stopped by a higher-rank incoming task its `state` transitions to `CANCELLED` and `terminal_reason` contains `"PREEMPTED_BY_HIGHER_RANK rank=N request=<id>"`.

Use `isPreempted(const TaskRecord&)` to test for this condition:

```cpp
if (isPreempted(record)) {
    // Task was displaced by a higher-priority request
}
```

## Rank rules

| Condition | Result |
|-----------|--------|
| `new.rank == 0` | Never preempts anything |
| `new.rank <= existing.rank` | Rejected normally (no preemption) |
| `new.rank > existing.rank` | Existing task(s) on the target device are cancelled, new task accepted |
| `rank` field absent in JSON | Decoder rejects the message (`nullopt`) |

## Dependencies

This library has minimal external dependencies. CMake resolves each automatically if not already installed.

| Library | Required | Resolution |
|---------|----------|------------|
| nlohmann/json ≥ 3.11 | YES | System package or auto-fetched via FetchContent |
| spdlog ≥ 1.13 | YES | System package, pkg-config, or auto-fetched via FetchContent |
| googletest 1.14 | Tests only | System package or auto-fetched via FetchContent |

**Install system packages (Ubuntu/Debian):**
```bash
sudo apt-get install -y nlohmann-json3-dev libspdlog-dev libfmt-dev
```

**Install system packages (CentOS/RHEL):**
```bash
sudo dnf install -y fmt-devel
# spdlog must be built from source on CentOS 10 — see Containerfile.test for exact steps
```

If packages are missing, CMake will print a `FATAL_ERROR` with the exact install command.

## Building

SdrTaskApi is a static library consumed via CMake `add_subdirectory`. It is not built standalone in production — it is always a dependency of `SdrResourceManager` and `AcquisitionApp`.

**As a dependency (recommended):** Place this repo as a sibling of the consumer repo. The consumer's `CMakeLists.txt` detects it automatically:

```
parent/
├── SdrTaskApi/       ← this repo
├── SdrResourceManager/
└── AcquisitionApp/
```

**For unit tests only (container — no setup needed):**

```bash
podman build -f Containerfile.test -t sdr-task-api:test .
podman run --rm sdr-task-api:test                              # exits 0 on pass
podman run --rm sdr-task-api:test ctest --output-on-failure -V # verbose
```

**Native build (using the included build script):**

```bash
./build.sh           # Release build
./build.sh --tests   # Release build + run unit tests
./build.sh --debug   # Debug build (AddressSanitizer + UBSan)
./build.sh --clean --tests  # Clean rebuild + tests
./build.sh --help    # All options
```

Or directly with CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

Tests cover 63 cases: type conversions, rank field defaults, `isPreempted`, `PREEMPT_TERMINAL_REASON`, message codec encode/decode for all `msg_type` values, and required-rank enforcement.

## Inter-repo dependencies

| Repo | Uses SdrTaskApi | How |
|------|-----------------|-----|
| [SdrResourceManager](https://github.com/BMichaud7/SdrResourceManager) | YES | `add_subdirectory(../SdrTaskApi)` or installed package |
| [AcquisitionApp](https://github.com/BMichaud7/AcquisitionApp) | YES | `add_subdirectory(../SdrTaskApi)` or installed package |

Both consumer repos expect SdrTaskApi as a sibling directory. Clone all three repos under the same parent folder.

## Repository

https://github.com/BMichaud7/SdrTaskApi
