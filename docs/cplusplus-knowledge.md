# engine_demo — C++ Knowledge & Constraints

> **Purpose**: CORE Rules + Examples pillars. Contains every C++ constraint, coding pattern,
> naming convention, and known defect that agents must honor when analyzing crashes,
> proposing fixes, or generating code in the `engine_demo` workspace.
>
> **Source**: `specs/constitution.md`, `.github/instructions/cpp-snippets.instructions.md`,
> observed patterns in `include/engine_demo/**/*.h` and `src/engine_demo/**/*.cpp`.

---

## 1. Constitutional articles (ground truth)

These 8 articles are non-negotiable. Any agent output that violates them is **automatically
rejected** by the reviewer chat mode.

### Article 1 — No exceptions

- Compile flags: `-fno-exceptions` (GCC/Clang), `/EHs-c-` (MSVC)
- Forbidden tokens: `try`, `catch`, `throw`, `noexcept(false)`, `std::exception`
- Error handling: return `nullptr`, status enum, or `eastl::expected<T, status>`

### Article 2 — No RTTI

- Compile flags: `-fno-rtti` (GCC/Clang), `/GR-` (MSVC)
- Forbidden tokens: `dynamic_cast`, `typeid`
- Type discrimination: tagged unions, `eastl::variant`, typed-handle patterns

### Article 3 — EASTL-first

- Default containers: `eastl::vector`, `eastl::string`, `eastl::unique_ptr`,
  `eastl::shared_ptr`, `eastl::function`, `eastl::span`, `eastl::optional`, `eastl::variant`
- `std::` containers permitted ONLY in interop layers marked `// interop boundary`
- Non-allocating `std::` is always allowed: `<cstdint>`, `<cstddef>`, `<utility>`,
  `<type_traits>`, `<cmath>`, `<cstring>`

### Article 4 — Allocator-aware

- Every EASTL container takes an explicit allocator at construction
- Source of truth: `engine_demo::allocator` in `include/engine_demo/allocator.h`
- Adapter: `engine_demo::eastl_allocator_ref` bridges to EASTL's allocator concept
- Default-constructed containers are **forbidden** in committed code

### Article 5 — Determinism

- Simulation accumulators: `double` (never `float` — this IS BUG-002)
- RNG: explicitly seeded `std::mt19937` (via `engine_demo::sim::rng`); never
  `std::random_device` in sim paths
- Container iteration: deterministic order; never `eastl::unordered_map` keyed on
  pointer identity in sim paths (this IS BUG-004)

### Article 6 — Real-time budgets

- Frame budget: ≤16.67 ms at 60 FPS, ≤8.33 ms at 120 FPS
- No allocation in inner loops; pool or arena up front
- No locks in render thread; use lockless ring buffers for cross-thread handoff
- Branch-prediction-friendly idioms preferred

### Article 7 — Test-first

- Every public function has at least one GTest covering happy + edge cases
- Implementation follows test (test file written first)
- CI gates on green tests + clang-tidy clean
- `DISABLED_` prefix = seeded bug regression (expected to fail until fix lands)

### Article 8 — HITL gates

- Spec-Kit pauses for human approval between `/plan` → `/tasks` and between each `/implement`
- Coding Agent handoffs require human review before merge
- Reviewer chat mode runs over every session bundle before delivery

---

## 2. Naming conventions (observed in codebase)

| Element | Convention | Examples |
|---------|-----------|----------|
| Namespaces | `snake_case`, nested | `engine_demo`, `engine_demo::ecs`, `engine_demo::physics`, `engine_demo::sim` |
| Classes / structs | `snake_case` | `constraint_solver`, `game_loop`, `frame_budget`, `entity_handle` |
| Member variables | `m_` prefix | `m_offset`, `m_bodies`, `m_accumulator_seconds`, `m_alloc` |
| Constants | `k` + PascalCase | `kMaxEntities`, `kTrailLength`, `kFixedStepSeconds` |
| Enum classes | `snake_case` type + values | `alloc_status::out_of_memory`, `scene_kind::rope` |
| Free functions | `snake_case` | `align_up`, `derive_subseed` |
| Macros | UPPER_SNAKE (avoid in new code) | — |
| Files | `snake_case.h` / `.cpp` | `game_loop.h`, `constraint.cpp` |

---

## 3. Required annotations

### `[[nodiscard]]`

Apply to:
- Factory functions (`make_*`, `create_*`)
- Functions returning a status / result / error type
- Functions whose only purpose is returning a value
- Classes that should never be silently discarded (e.g., `class [[nodiscard]] allocator`)

### `noexcept`

Apply to:
- Move constructors and move-assignment operators
- Swap functions
- Destructors (implicit, but never annotate otherwise)
- All methods in this codebase (since exceptions are disabled globally)

### `// interop boundary`

Required comment on any line or block that uses `std::` types that normally violate Article 3.
Current interop boundaries in the codebase:
- `sim::rng` — uses `std::mt19937` (EASTL has no Mersenne Twister)
- Sandbox `app.cpp` — uses raylib C API (labeled interop boundary)

---

## 4. Error handling patterns (no exceptions)

```text
Pattern 1: Return nullptr
  void* allocate(size_t n, size_t alignment) noexcept {
      if (bad_input) return nullptr;
      ...
  }

Pattern 2: Return status enum
  enum class alloc_status : uint8_t { ok, out_of_memory, invalid_alignment, invalid_size };

Pattern 3: Guard-clause early returns
  void destroy_entity(entity_handle h) noexcept {
      if (h.index >= kMaxEntities) return;
      auto& s = m_slots[h.index];
      if (!s.alive || s.generation != h.generation) return;
      // ... proceed with valid state
  }

Pattern 4: Sentinel / default values
  entity_handle{} — index=0, generation=0, is_valid()=false
  const body* try_get_body(id) — returns nullptr if not found
```

---

## 5. Seeded bugs catalog

Six intentionally planted defects. **Do NOT fix** — they are the demo material.

### BUG-001 — Allocator OOB write (Session 02 primary)

- **File**: `src/engine_demo/allocator.cpp`
- **CWE**: CWE-787 (Out-of-bounds Write)
- **Mechanism**: Bounds check uses pre-alignment offset (`m_offset + n > m_capacity`)
  but alignment padding computed AFTER the check can push the actual write past the arena tail
- **Trigger**: Third aligned allocation against a tight arena
- **Symptom**: Crash (access violation / segfault)
- **Fixture**: `fixtures/crash_dumps/BUG-001/` (`.dmp` + `.pdb`)
- **Fix**: Compute aligned offset FIRST, then check `(aligned_offset + n) <= m_capacity`

### BUG-002 — Game loop float drift (Session 03)

- **File**: `src/engine_demo/sim/game_loop.cpp`
- **CWE**: CWE-681 (Incorrect Conversion)
- **Mechanism**: Accumulator is `float` instead of `double`; implicit narrowing on every
  `+=` accumulates representation error at ~ULP(float)/step
- **Trigger**: ~30 seconds of simulated time at 1/60s steps
- **Symptom**: Substep count oscillates; replay diverges from reference
- **Fix**: Change `m_accumulator_seconds` from `float` to `double`

### BUG-003 — ECS use-after-free (Session 04 security)

- **File**: `src/engine_demo/ecs/world.cpp`
- **CWE**: CWE-416 (Use After Free)
- **Mechanism**: `destroy_entity()` sets `alive=false` but does NOT bump `generation`.
  Next `create_entity()` reuses the slot with same generation — stale handles pass `is_alive()`
- **Trigger**: Destroy entity, create new entity that recycles same slot, use old handle
- **Symptom**: Stale handle accesses wrong logical entity's data
- **Fix**: Add `++s.generation;` in `destroy_entity()` before setting `alive=false`

### BUG-004 — Physics non-determinism (Session 04 logic)

- **File**: `src/engine_demo/physics/constraint.cpp`
- **CWE**: N/A (determinism violation)
- **Mechanism**: `solve()` reads body state via `m_bodies.find()` on an `eastl::hash_map`.
  Hash collisions differ across runs → projection order changes → solver converges to
  slightly different positions
- **Trigger**: Multiple bodies with hash-colliding IDs; different run = different order
- **Symptom**: Replay diverges by O(1e-9) per frame, accumulating over seconds
- **Fix**: Sort constraints by `(min(a,b), max(a,b))` at insertion; read body state via
  deterministic index instead of hash-map lookup

### BUG-005 — RNG seed truncation (Session 04 security)

- **File**: `src/engine_demo/sim/rng.cpp`
- **CWE**: CWE-197 (Numeric Truncation Error)
- **Mechanism**: `uint64_t seed` forwarded as `static_cast<uint32_t>(seed)` to `mt19937`
  constructor — high 32 bits silently dropped
- **Trigger**: Two seeds sharing low 32 bits produce identical RNG streams
- **Symptom**: Collision in per-session entropy; security + determinism regression
- **Fix**: Use `derive_subseed(seed)` which XOR-folds high half into low half

### BUG-006 — Frame budget off-by-one (Session 03)

- **File**: `src/engine_demo/frame_budget.cpp`
- **CWE**: CWE-193 (Off-by-one Error)
- **Mechanism**: `record_sample()` writes at `m_index` then increments; `rolling_average()`
  iterates `[0, m_count)`. On first frame, slot 0 is counted twice (written + initial zero
  not cleared)
- **Trigger**: First frame after construction
- **Symptom**: Non-zero average from an idle engine
- **Fix**: Pre-increment index before write, OR track `m_count` as `min(writes, window_size)`
  and iterate from `(m_index - m_count)` wrapping

---

## 6. False-positive catalog (Session 04 triage exercise)

### FP-001 — `derive_subseed` appears to truncate

- **File**: `src/engine_demo/sim/rng.cpp`
- **Why it looks suspicious**: Function takes `uint64_t`, returns `uint32_t` — looks like
  the same pattern as BUG-005
- **Why it's safe**: XOR-folds `high ^ low` — preserves entropy from both halves
- **Learner action**: Dismiss with a one-line comment near the suspect line

### FP-002 — Constraint solver loop overrun

- **File**: `src/engine_demo/physics/constraint.cpp`
- **Why it looks suspicious**: Trailing iteration appears to read past `m_constraints.end()`
- **Why it's safe**: Sentinel iteration exits before any buffer read
- **Learner action**: Dismiss with a one-line comment near the suspect line

---

## 7. Interop boundaries

| Location | `std::` type used | Reason |
|----------|-------------------|--------|
| `sim::rng` | `std::mt19937` | EASTL does not ship a Mersenne Twister engine |
| `sim::rng` | `std::mt19937::result_type` | Required for seed type |
| `frame_budget` | `double[256]` (C array) | Zero-allocation ring; not an EASTL container |
| Sandbox `app.cpp` | raylib C API | Rendering interop boundary (not engine_demo library) |
| Headers | `<cstdint>`, `<cstddef>`, `<cstring>`, `<cmath>` | Non-allocating standard library (always allowed) |

---

## 8. Compilation requirements

### Flags (enforced by `cmake/CompilerOptions.cmake`)

| Flag | Platform | Effect |
|------|----------|--------|
| `-fno-exceptions` | GCC / Clang | Disables exception support |
| `/EHs-c-` | MSVC | Disables exception support |
| `-fno-rtti` | GCC / Clang | Disables RTTI |
| `/GR-` | MSVC | Disables RTTI |
| `-Wall -Wextra -Werror` | GCC / Clang | All warnings as errors |
| `/W4 /WX` | MSVC | All warnings as errors |
| `-Wsign-conversion` | GCC / Clang | Signed/unsigned mismatch |
| `-Wunused-private-field` | Clang | Unused member detection |

### Standards

- C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`)
- No extensions (`CMAKE_CXX_EXTENSIONS OFF`)

### Supported compilers

- MSVC 19.38+ (Visual Studio 2022 17.8+)
- GCC 13+
- Clang 17+ (Apple Clang 17+ on macOS)

---

## 9. Anti-pattern checklist (what agents must NEVER produce)

Agents (crash-engineer, crash-qa, crash-validator) must reject or rewrite any code
containing these patterns:

| Anti-pattern | Required replacement |
|-------------|---------------------|
| `std::vector<T>` | `eastl::vector<T, eastl_allocator_ref>` |
| `std::string` | `eastl::string` (with allocator) |
| `std::unique_ptr<T>` | `eastl::unique_ptr<T>` |
| `std::map<K,V>` / `std::unordered_map<K,V>` | `eastl::hash_map<K,V,...,eastl_allocator_ref>` |
| `try` / `catch` / `throw` | Status enum return or `nullptr` |
| `dynamic_cast<T*>(x)` | Typed-handle pattern or `eastl::variant` |
| `(T)x` (C-style cast) | `static_cast<T>(x)` |
| `new T` / `delete p` | `eastl::make_unique<T>(...)` or allocator |
| `float` for sim accumulator | `double` |
| `std::random_device` in sim | `engine_demo::sim::rng` with explicit seed |
| `using namespace std;` | Never (especially in headers) |
| `#include <bits/stdc++.h>` | Include only what you use |
| Default-constructed EASTL container | Pass explicit `eastl_allocator_ref` |
| `typedef` | `using` alias |
| `std::bind` | Lambda |

---

## 10. Code idioms to prefer

| Idiom | When |
|-------|------|
| Range-for | Iterating any container |
| Structured bindings | Tuple/pair returns, hash_map iteration |
| `if constexpr` | Compile-time branches (over SFINAE) |
| `eastl::span<T>` | Non-owning array views in function signatures |
| C++20 Concepts | Template constraints (document intent) |
| `constexpr` | Wherever feasible for compile-time evaluation |
| `constinit` | Global state with non-trivial init |
| Guard clauses | Early-return for invalid input (not deep nesting) |

---

## 11. Crash-dump analysis context (Session 02 specific)

When analyzing a crash dump from this codebase, agents should know:

1. **The allocator is a bump-pointer arena** — deallocate is a no-op. Memory is only
   reclaimed when the allocator object is destroyed. An OOB write in the arena corrupts
   adjacent allocations, not freed memory.

2. **Entity handles are generational** — a stale handle crash means the generation check
   failed (BUG-003) or was bypassed.

3. **The physics solver mutates bodies in-place** — a crash in `solve()` likely means a
   body was removed mid-iteration or the hash_map was invalidated.

4. **RNG state is per-instance** — if two systems share an `rng` instance, their streams
   are entangled (not a bug, but a common confusion point).

5. **Frame budget never allocates at runtime** — if you see an allocation crash in
   `frame_budget`, it happened during construction, not during `record_sample()`.

6. **The sandbox is an interop boundary** — crashes in `app.cpp` may involve raylib's
   internal state, not engine_demo logic. Distinguish raylib frames from engine_demo frames
   in the call stack.
