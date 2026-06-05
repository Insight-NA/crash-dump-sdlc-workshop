# engine_demo — System Architecture Knowledge

> **Purpose**: CORE Context pillar. Provides agents with full architectural understanding
> of the `engine_demo` workspace so they can reason correctly about crash locations,
> data flow, module boundaries, and fix scope.
>
> **Source**: derived from `include/engine_demo/**/*.h`,
> `src/engine_demo/**/*.cpp`, `specs/constitution.md`,
> and `fixtures/seeded-bugs.md`.

---

## 1. System overview

`engine_demo` is a **synthetic C++20 game-engine library** built for the EA × Insight
GitHub Copilot workshop. It is intentionally small (6 translation units + headers) so
that agents and learners can hold the entire codebase in context during a 60-minute session.

The library models five subsystems commonly found in AAA game engines:

| Subsystem              | Header                 | Source                   | Purpose                                                       |
| ---------------------- | ---------------------- | ------------------------ | ------------------------------------------------------------- |
| Allocator              | `allocator.h`          | `allocator.cpp`          | Bump-pointer arena over a caller-provided buffer              |
| ECS                    | `ecs/world.h`          | `ecs/world.cpp`          | Minimal entity-component system with generational handles     |
| Physics                | `physics/constraint.h` | `physics/constraint.cpp` | Position-based constraint solver over a hash-keyed body table |
| Simulation — Game Loop | `sim/game_loop.h`      | `sim/game_loop.cpp`      | Fixed-step accumulator with configurable substep cap          |
| Simulation — RNG       | `sim/rng.h`            | `sim/rng.cpp`            | Deterministic Mersenne Twister wrapper                        |
| Frame Budget           | `frame_budget.h`       | `frame_budget.cpp`       | Rolling-window frame timing statistics                        |

Additionally, `src/engine_demo/eastl_overrides.cpp` provides the mandatory EASTL global
`operator new[]` overrides required for linking.

---

## 2. Module dependency graph

```text
                  ┌──────────────────────┐
                  │  engine_demo::allocator  │
                  │  (allocator.h / .cpp)    │
                  └──────────┬───────────┘
                             │
             ┌───────────────┼───────────────────┐
             │               │                   │
             ▼               ▼                   ▼
   ┌─────────────────┐  ┌──────────────┐  ┌──────────────────┐
   │ ecs::world      │  │ frame_budget │  │ eastl_allocator_ref │
   │ (world.h/.cpp)  │  │ (.h/.cpp)    │  │ (allocator.h)       │
   └─────────────────┘  └──────────────┘  └────────┬───────────┘
                                                    │
                                                    ▼
                                          ┌─────────────────────────┐
                                          │ physics::constraint_solver │
                                          │ (constraint.h / .cpp)      │
                                          └─────────────────────────┘

   ┌──────────────────┐   ┌──────────────────┐
   │ sim::game_loop   │   │ sim::rng         │
   │ (game_loop.h/.cpp│   │ (rng.h / .cpp)   │
   │  STANDALONE)     │   │  STANDALONE      │
   └──────────────────┘   └──────────────────┘
```

**Key observations**:

- `allocator` is the root dependency — everything that allocates depends on it.
- `eastl_allocator_ref` is the bridge type: an EASTL-compatible adapter wrapping `allocator*`.
- `sim::game_loop` and `sim::rng` are **standalone** — no allocator dependency. They take
  their state at construction and evolve it via pure methods.
- `physics::constraint_solver` uses EASTL containers (`hash_map`, `vector`) wired through
  `eastl_allocator_ref`.

---

## 3. Public API surface

### 3.1 `engine_demo::allocator` (`include/engine_demo/allocator.h`)

```text
class allocator:
  ctor(void* buffer, std::size_t capacity_bytes) noexcept
  move-ctor / move-assign noexcept
  allocate(n, alignment = max_align_t) → void* | nullptr    noexcept
  allocate(n, alignment, offset) → void* | nullptr          noexcept
  deallocate(p, n) → void                                   noexcept [no-op in bump arena]
  bytes_used() → std::size_t                                noexcept
  capacity() → std::size_t                                  noexcept
  get_name() / set_name(const char*)                        noexcept
```

### 3.2 `engine_demo::eastl_allocator_ref` (`include/engine_demo/allocator.h`)

```text
class eastl_allocator_ref:
  default-ctor noexcept [EASTL requires this]
  ctor(allocator& alloc) noexcept [explicit]
  allocate(n, flags=0) → void*                              noexcept
  allocate(n, alignment, offset, flags=0) → void*           noexcept
  deallocate(p, n) → void                                   noexcept
  get_name() / set_name(const char*)                        noexcept
  underlying() → allocator*                                 noexcept
```

### 3.3 `engine_demo::ecs::world` (`include/engine_demo/ecs/world.h`)

```text
struct entity_handle { uint32_t index; uint32_t generation; is_valid() }
operator==(entity_handle, entity_handle)

class world:
  ctor(allocator& alloc) noexcept
  create_entity() → entity_handle                           noexcept [[nodiscard]]
  destroy_entity(entity_handle h) → void                    noexcept
  is_alive(entity_handle h) → bool                          noexcept [[nodiscard]]
  live_count() → std::size_t                                noexcept [[nodiscard]]

  Internal: kMaxEntities = 4096, fixed-slot array, linear scan for free
```

### 3.4 `engine_demo::physics::constraint_solver` (`include/engine_demo/physics/constraint.h`)

```text
struct body { id: uint64, position_xyz: double×3, inverse_mass: double }
struct distance_constraint { a: uint64, b: uint64, rest_length: double }

class constraint_solver:
  ctor(allocator& alloc) noexcept
  add_body(body b) → void                                   noexcept
  add_constraint(distance_constraint c) → void              noexcept
  solve(max_iterations: uint32) → uint32 [iterations spent] noexcept [[nodiscard]]
  try_get_body(id: uint64) → const body* | nullptr          noexcept [[nodiscard]]

  Internal: eastl::hash_map<uint64,body> m_bodies
            eastl::vector<distance_constraint> m_constraints
            Both wired through eastl_allocator_ref
```

### 3.5 `engine_demo::sim::game_loop` (`include/engine_demo/sim/game_loop.h`)

```text
struct game_loop_config { fixed_step_seconds: double = 1/60; max_substeps: double = 8 }

class game_loop:
  ctor(game_loop_config cfg) noexcept
  advance(delta_seconds: double) → uint32 [substeps taken]  noexcept [[nodiscard]]
  accumulator_seconds() → double                            noexcept [[nodiscard]]

  Internal: m_accumulator_seconds is FLOAT (BUG-002 — should be double)
```

### 3.6 `engine_demo::sim::rng` (`include/engine_demo/sim/rng.h`)

```text
class rng:
  ctor(seed: uint64) noexcept
  next_u32() → uint32                                       noexcept [[nodiscard]]
  next_double_unit() → double [0.0, 1.0)                    noexcept [[nodiscard]]

  Internal: std::mt19937 m_engine (interop boundary — EASTL has no MT engine)
  BUG-005: seed static_cast<uint32_t> drops high 32 bits
```

### 3.7 `engine_demo::frame_budget` (`include/engine_demo/frame_budget.h`)

```text
class frame_budget:
  ctor(allocator& alloc, window_size: size_t = 64) noexcept
  record_sample(milliseconds: double) → void                noexcept
  rolling_average() → double                                noexcept [[nodiscard]]
  sample_count() → std::size_t                              noexcept [[nodiscard]]
  get_allocator() → const allocator&                        noexcept [[nodiscard]]

  Internal: fixed double[256] ring buffer, window_size ≤ 256
  BUG-006: off-by-one in ring index advancement
```

---

## 4. Design patterns

| Pattern                   | Where                 | Description                                                                       |
| ------------------------- | --------------------- | --------------------------------------------------------------------------------- |
| Bump-pointer arena        | `allocator`           | Linear allocator; deallocate is no-op; reclaim by destroying the allocator        |
| Generational handles      | `ecs::world`          | Slot + generation counter; stale handles detected by generation mismatch          |
| Position-based dynamics   | `constraint_solver`   | Iterative constraint projection; bodies are directly mutated per iteration        |
| Fixed-step accumulator    | `game_loop`           | Classic game-physics pattern: accumulate wall-clock dt, drain in fixed-size bites |
| Rolling-window statistics | `frame_budget`        | Circular buffer of N most recent samples; O(1) record, O(N) average               |
| EASTL allocator adapter   | `eastl_allocator_ref` | Bridge between engine_demo's allocator and EASTL's allocator concept              |

---

## 5. Build system

```text
Build tool:   CMake 3.28+ / Ninja generator
Package mgr:  vcpkg manifest mode (vcpkg.json at root)
Presets:      default-debug, default-release (CMakePresets.json)
Languages:    CXX (C++20), OBJC (macOS only, for sandbox force_visible.m)
```

### Dependencies (vcpkg.json)

| Package  | Purpose                                                              |
| -------- | -------------------------------------------------------------------- |
| `eastl`  | Containers, smart pointers, allocator concept                        |
| `gtest`  | Unit tests (GoogleTest + GoogleMock)                                 |
| `raylib` | Sandbox graphical rendering (guarded by `ENGINE_DEMO_BUILD_SANDBOX`) |

### CMake structure

```text
CMakeLists.txt (root)
├── cmake/CompilerOptions.cmake   — -fno-exceptions, -fno-rtti, -Wall -Werror
├── cmake/Dependencies.cmake      — find_package(EASTL), find_package(GTest), find_package(raylib)
├── src/CMakeLists.txt            — engine_demo static library
├── tests/CMakeLists.txt          — GTest executables
└── apps/CMakeLists.txt           — ea-sandbox executable (guarded by option)
    └── apps/sandbox/CMakeLists.txt
```

---

## 6. Executable targets

### `ea-sandbox` (`apps/sandbox/`)

Interactive physics visualizer + headless determinism tracer.

| File                            | Role                                               |
| ------------------------------- | -------------------------------------------------- |
| `main.cpp`                      | CLI parsing, mode dispatch                         |
| `app.cpp`                       | raylib renderer (interop boundary)                 |
| `headless.cpp`                  | CSV trace output, no graphics                      |
| `scene.h` / `scene.cpp`         | Four physics scenes (rope, pendulum, cloth, storm) |
| `telemetry.h` / `telemetry.cpp` | JSONL frame-graph + event stream                   |
| `force_visible.m`               | macOS Obj-C window-forcing (Apple only)            |

**CLI flags**: `--seed N`, `--frames N`, `--headless`, `--scene rope|pendulum|cloth|storm`,
`--screenshot PATH [--warmup N]`, `--debug`, `--telemetry-level off|frame|verbose`,
`--telemetry-out PATH`, `--watchdog`, `--no-crash-handlers`.

**Determinism contract**: `--headless --seed 42 --frames 60` produces a stable digest
per scene (e.g., rope = `33a6319d856d4869`).

---

## 7. Test infrastructure

```text
tests/
└── engine_demo/
    ├── allocator_test.cpp      — arena happy-path + edge cases
    ├── ecs_world_test.cpp      — entity lifecycle, generation, pool exhaustion
    ├── constraint_test.cpp     — solver convergence, body lookup
    ├── game_loop_test.cpp      — substep counts, accumulator precision
    ├── rng_test.cpp            — seed determinism, distribution
    └── frame_budget_test.cpp   — rolling average, window boundary
```

**Convention**: tests prefixed with `DISABLED_` are **seeded-bug regression tests**.
They are expected to fail against the current (buggy) code and pass after the bug is fixed
on stage. CI runs them with `GTEST_ALSO_RUN_DISABLED_TESTS=1` to confirm they still fail.

---

## 8. Fixtures

```text
fixtures/
├── crash_dumps/
│   └── BUG-001/            — .dmp + .pdb for the allocator OOB crash
├── bug_reports/            — structured reports for BUG-002..006
└── seeded-bugs.md          — master catalog (6 bugs + 2 false positives)
```

The Session 02 crash-dump SDLC workflow uses `fixtures/crash_dumps/BUG-001/` as its primary
input. The `.dmp` is opened with `cdb.exe` (Windows Debugging Tools) via the `crash-dump-mcp`
server's `parse_minidump` tool.

---

## 9. Data flow for a crash-dump session (Session 02)

```text
.dmp file (fixture)
    │
    ▼
crash-dump-mcp::parse_minidump → raw call stack + registers
    │
    ▼
crash-dump-mcp::resolve_symbols + .pdb → symbolicated stack
    │
    ▼
crash-analyzer agent → Tree-of-Thought analysis → 3 hypotheses
    │
    ▼
HITL gate #1 → human selects hypothesis
    │
    ▼
crash-planner agent → resolution brief (fix strategy)
    │
    ▼
HITL gate #2 → human approves plan
    │
    ▼
crash-engineer agent (×3 worktrees) → parallel fix implementations
    │
    ▼
crash-qa agent → regression tests per implementation
    │
    ▼
crash-validator agent → determinism check + constitution compliance
    │
    ▼
HITL gate #3 → human merges winning PR
```

---

## 10. Key file paths (quick reference)

| What                | Path (relative to repo root)               |
| ------------------- | ------------------------------------------ |
| Allocator header    | `include/engine_demo/allocator.h`          |
| ECS header          | `include/engine_demo/ecs/world.h`          |
| Physics header      | `include/engine_demo/physics/constraint.h` |
| Game loop header    | `include/engine_demo/sim/game_loop.h`      |
| RNG header          | `include/engine_demo/sim/rng.h`            |
| Frame budget header | `include/engine_demo/frame_budget.h`       |
| Constitution        | `specs/constitution.md`                    |
| Bug catalog         | `fixtures/seeded-bugs.md`                  |
| CMake root          | `CMakeLists.txt`                           |
| vcpkg manifest      | `vcpkg.json`                               |
| Sandbox entry       | `apps/sandbox/main.cpp`                    |
