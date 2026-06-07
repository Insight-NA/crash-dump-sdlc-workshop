# BUG-001 — Implementation Notes
## Branch: `bugfix/BUG-001-deferred-init-member`

**Engineer:** crash-fix-engineer  
**Date:** 2026-06-07  
**Hypothesis addressed:** H3 — Deferred-init member written before assignment (AGENTS.md Art. 4)

---

## 1. Audit Findings

The H3 hypothesis predicted that a struct pointer member was zero-initialized at construction
but written through before a later `init()`/`setup()` call populated it.

A full audit of all engine_demo headers, sandbox app sources, and fixture files was performed.

### Two-phase construction patterns found

| Class | Pattern | Write-path guarded? | Action |
|---|---|---|---|
| `ea_sandbox::telemetry` | Singleton + `init(telemetry_options)` method; `m_file{nullptr}` | ✅ All `emit_*` guard `m_file == nullptr` | No change needed |
| `engine_demo::frame_budget` | Single-phase ctor (takes `allocator&`); `m_alloc` has **no** `{nullptr}` default | ⚠️ `get_allocator()` dereferences `m_alloc`; indeterminate after move | **Fixed** |
| `engine_demo::ecs::world` | Single-phase ctor (takes `allocator&`); `m_alloc` has **no** `{nullptr}` default | ⚠️ Member stored but never dereferenced; still indeterminate after move | **Fixed** |
| `engine_demo::physics::constraint_solver` | Single-phase ctor; all members have default initializers | ✅ | No change needed |
| `engine_demo::sim::game_loop` | Single-phase ctor; all members have default initializers | ✅ | No change needed |
| `engine_demo::sim::rng` | Single-phase ctor; all members have default initializers | ✅ | No change needed |

### Explicit null-write in `apps/sandbox/app.cpp`
`trigger_demo_crash()` (line 547) contains a deliberate `volatile int* p = nullptr; *p = 0xDEAD;`
— this is intentional demo crash material for Session 02 and is entirely unrelated to BUG-001.
Not modified.

### Conclusion on H3 hypothesis
No confirmed deferred-init write-path AV bug was found independently of the BUG-001
allocator OOB (which is the confirmed root cause). The H3 branch delivers **defensive
hardening**: explicit `{nullptr}` default member initializers on the two raw pointer
members that lacked them, eliminating any indeterminate state in moved-from objects.

---

## 2. Changes Summary

| File | Change | Lines |
|---|---|---|
| `include/engine_demo/frame_budget.h` | Add `{nullptr}` to `m_alloc`; expand `get_allocator()` comment | +5 / -2 |
| `include/engine_demo/ecs/world.h` | Add `{nullptr}` to `m_alloc` | +2 / -1 |

**Commit:** `c196eef` on `bugfix/BUG-001-deferred-init-member`

### `frame_budget.h` — before:
```cpp
private:
    allocator* m_alloc;   // ← no default: indeterminate in moved-from state
```

### After:
```cpp
private:
    // H3: explicit {nullptr} default prevents indeterminate state in moved-from objects.
    allocator* m_alloc{nullptr};
```

### `world.h` — before:
```cpp
[[maybe_unused]] allocator* m_alloc;   // ← no default
```

### After:
```cpp
// H3: explicit {nullptr} default prevents indeterminate state in moved-from objects.
[[maybe_unused]] allocator* m_alloc{nullptr};
```

---

## 3. Build Status

**Toolchain:** MSVC 19.51 (VS 2026 Enterprise)  
**Targets built:** `engine_demo` + `test_allocator` + `test_frame_budget` + `test_world`

| Metric | Result |
|---|---|
| Compile errors | **0** |
| Linker errors | **0** |
| New warnings introduced by this fix | **0** |

### Pre-existing warnings (not introduced by H3)
`D9025: overriding '/EHs' with '/EHs-'` on all TUs — CMake flag ordering artifact
from `-fno-exceptions` enforcement. Present on `main` before any changes.

### Pre-existing build failure
`apps/sandbox/test-minimal.exe` — missing `glfw3.lib`. Same failure as H1/H2; out of scope.

---

## 4. Test Results

**Command:** `ctest --test-dir <build> --tests-regex "test_allocator|test_frame_budget|test_world" -V`

| Test suite | Tests run | Passed | Disabled (pre-existing) |
|---|---|---|---|
| `test_allocator` | 3 | ✅ 3 | 0 |
| `test_frame_budget` | 2 | ✅ 2 | 1 (`DISABLED_first_sample_is_not_double_counted_on_warmup` — BUG-006 placeholder) |
| `test_world` | 1 | ✅ 1 | 1 (`DISABLED_stale_handle_does_not_revive_after_recycle` — BUG-003 placeholder) |

**6 / 6 active tests passed. 0 regressions.**

---

## 5. Constitution Compliance

| Rule | Status |
|---|---|
| No `std::` containers | ✅ |
| No `throw` / `try` / `catch` | ✅ |
| No `dynamic_cast` / `typeid` | ✅ |
| No raw `new` / `delete` | ✅ — no allocation in this change |
| `[[nodiscard]]` on factories/status (Art. 5) | ✅ — unchanged |
| `noexcept` on moves (Art. 6) | ✅ — `frame_budget(frame_budget&&) noexcept = default` unchanged; `{nullptr}` default does not affect noexcept |
| Allocator-aware (Art. 4) | ✅ — `{nullptr}` makes the invariant explicit; constructor still sets the live pointer |

---

## 6. Self-Test Checklist

- [x] Code compiles with zero errors in the worktree
- [x] Pre-existing `D9025` warnings documented; no new warnings introduced
- [x] 6/6 active tests pass; 2 pre-existing `DISABLED` tests documented
- [x] No `std::` containers, no exceptions, no RTTI
- [x] No raw `new`/`delete`
- [x] Implementation notes written

---

**Handoff to:** `crash-qa`  
**Branch ready for QA:** `bugfix/BUG-001-deferred-init-member`  
**Worktree path:** `.worktrees/bugfix-BUG-001-deferred-init-member`
