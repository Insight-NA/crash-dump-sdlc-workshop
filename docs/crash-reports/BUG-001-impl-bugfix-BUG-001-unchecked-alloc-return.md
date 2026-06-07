# BUG-001 — Implementation Notes
## Branch: `bugfix/BUG-001-unchecked-alloc-return`

**Engineer:** crash-fix-engineer  
**Date:** 2026-06-06  
**Hypothesis addressed:** H2 — Unchecked allocate() return values; raw `new`/`delete` (CWE-252 / AGENTS.md Art. 5, 7)

---

## 1. Changes Summary

### Audit findings

| Location | Finding | Severity |
|---|---|---|
| `allocator.h` — `allocate()` overloads | `[[nodiscard]]` already present | ✅ No action |
| `allocator.h` — `eastl_allocator_ref::allocate()` | `[[nodiscard]]` already present | ✅ No action |
| `fixtures/crash_dumps/BUG-001/capture_self.cpp:66–68` | `p1` and `p2` return values stored but never null-checked before use in `printf`; `p3` was correctly checked | ⚠️ Fixed |
| `fixtures/crash_dumps/BUG-001/repro.cpp:61–63` | Same pattern as `capture_self.cpp` | ⚠️ Fixed |
| `apps/sandbox/scene.cpp:43` | Raw `new unsigned char[kArenaBytes]()` — violates AGENTS.md Art. 7; no null check on return | ⚠️ Fixed |
| `apps/sandbox/scene.cpp:60` | `delete[] m_arena_buffer` — matching raw `delete[]` | ⚠️ Fixed |
| All engine subsystem sources (`src/`) | No direct `allocate()` calls — all use EASTL containers via `eastl_allocator_ref` | ✅ No action |

### Files modified

| File | Change | Lines |
|---|---|---|
| `fixtures/crash_dumps/BUG-001/capture_self.cpp` | Add null-check guards on `p1` and `p2` after `allocate()` | +4 |
| `fixtures/crash_dumps/BUG-001/repro.cpp` | Add null-check guards on `p1` and `p2` after `allocate()` | +4 |
| `apps/sandbox/scene.h` | Add `#include <EASTL/unique_ptr.h>`; change `m_arena_buffer` from `unsigned char*` to `eastl::unique_ptr<unsigned char[]>` | +3 / -1 |
| `apps/sandbox/scene.cpp` | Use `eastl::make_unique<unsigned char[]>(kArenaBytes)` in ctor; replace `delete[]` destructor with `= default` | +1 / -3 |

**Commit:** `099226f` on `bugfix/BUG-001-unchecked-alloc-return`

### Diffs

**`capture_self.cpp` / `repro.cpp` — before:**
```cpp
void* p1 = a.allocate(40, 32);
void* p2 = a.allocate(40, 32);
void* p3 = a.allocate(40, 32);
```

**After:**
```cpp
void* p1 = a.allocate(40, 32);
if (!p1) { printf("p1 allocation failed (OOM).\n"); return 1; }
void* p2 = a.allocate(40, 32);
if (!p2) { printf("p2 allocation failed (OOM).\n"); return 1; }
void* p3 = a.allocate(40, 32);
```

**`scene.h` — member type change:**
```cpp
// Before (raw pointer, no [[nodiscard]] safety net):
unsigned char* m_arena_buffer{nullptr};

// After (Art. 7 compliant):
eastl::unique_ptr<unsigned char[]> m_arena_buffer;
```

**`scene.cpp` — constructor / destructor:**
```cpp
// Before:
: m_arena_buffer{new unsigned char[kArenaBytes]()},
  m_alloc{m_arena_buffer, kArenaBytes},
// ...
scene::~scene() { delete[] m_arena_buffer; }

// After:
: m_arena_buffer{eastl::make_unique<unsigned char[]>(kArenaBytes)},
  m_alloc{m_arena_buffer.get(), kArenaBytes},
// ...
scene::~scene() = default;
```

---

## 2. Build Status

**Toolchain:** MSVC 19.51 (VS 2026 Enterprise)  
**Target built:** `engine_demo` + `test_allocator`

| Metric | Result |
|---|---|
| Compile errors | **0** |
| Linker errors (test targets) | **0** |
| New warnings introduced by this fix | **0** |

### Pre-existing warnings (not introduced by this fix)
All translation units emit `cl: Command line warning D9025: overriding '/EHs' with '/EHs-'` — this is a CMake flag ordering artifact from the project's `-fno-exceptions` enforcement. Present on `main` before any changes; not introduced by H2. Flagged for QA agent.

### Pre-existing linker failure (not introduced by this fix)
`apps/sandbox/test-minimal.exe` fails to link (missing `glfw3.lib` for raylib symbols). Same failure as documented in H1 notes.

---

## 3. Test Results

**Command:** `ctest --test-dir <build> --tests-regex test_allocator -V`

| Test | Result |
|---|---|
| `allocator.basic_allocation_returns_aligned_pointer` | ✅ PASSED |
| `allocator.exhaustion_returns_null` | ✅ PASSED |
| `allocator.third_aligned_alloc_does_not_overrun_arena` | ✅ PASSED |

**3 / 3 passed. 0 failed.**

---

## 4. Constitution Compliance

| Rule | Status |
|---|---|
| No `std::` containers | ✅ — `eastl::unique_ptr` / `eastl::make_unique` used |
| No `throw` / `try` / `catch` | ✅ |
| No `dynamic_cast` / `typeid` | ✅ |
| No raw `new` / `delete` (Art. 7) | ✅ — `new[]`/`delete[]` replaced with `eastl::unique_ptr` |
| `[[nodiscard]]` on `allocate()` (Art. 5) | ✅ — already present; call sites now check return values |
| `noexcept` on moves | ✅ — `eastl::unique_ptr` move ctor is `noexcept` |
| Explicit allocator parameter | ✅ — `m_alloc` constructed with `.get()` raw pointer from `unique_ptr` |

---

## 5. Self-Test Checklist

- [x] Code compiles with zero errors in the worktree
- [x] Pre-existing `D9025` warnings documented; no new warnings introduced by this fix
- [x] All allocator tests pass; pre-existing build failures documented
- [x] No `std::` containers, no exceptions, no RTTI
- [x] No raw `new`/`delete` — replaced with `eastl::unique_ptr`
- [x] Implementation notes written

---

**Handoff to:** `crash-qa`  
**Branch ready for QA:** `bugfix/BUG-001-unchecked-alloc-return`  
**Worktree path:** `.worktrees/bugfix-BUG-001-unchecked-alloc-return`
