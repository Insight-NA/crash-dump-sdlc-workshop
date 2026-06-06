# BUG-001 — Implementation Notes
## Branch: `bugfix/BUG-001-null-write-destination`

**Engineer:** crash-fix-engineer  
**Date:** 2026-06-06  
**Hypothesis addressed:** H1 — OOB write in arena allocator (CWE-787)

---

## 1. Changes Summary

### Root cause (confirmed)
`allocator::allocate()` in `src/engine_demo/allocator.cpp` advanced `m_offset` by `n` and then applied alignment, meaning the bounds check used the raw `m_offset + n` without accounting for alignment padding. On the third allocation of a 40-byte, 32-byte-aligned block into a 160-byte arena, alignment padding caused the total consumed bytes to silently exceed `m_capacity`, producing an OOB write (CWE-787).

### Fix (commit `f66466f`)
| File | Change |
|---|---|
| `src/engine_demo/allocator.cpp` | Moved `align_up(m_offset, alignment)` **before** the bounds check; gate is now `aligned + n > m_capacity`; `m_offset` advances to `aligned + n` atomically. |

**Lines changed:** 4 lines removed, 5 lines added (net +1).  
**No other files modified.**

### Pre-fix code
```cpp
if (m_offset + n > m_capacity) {      // ← bounds check ignored alignment padding
    return nullptr;
}
m_offset = align_up(m_offset, alignment);
std::byte* ptr = m_buffer + m_offset;
m_offset += n;
return ptr;
```

### Fixed code
```cpp
// Fix BUG-001: compute aligned offset BEFORE the bounds check so that
// alignment padding is correctly included in the capacity test.
const auto aligned = align_up(m_offset, alignment);
if (aligned + n > m_capacity) {
    return nullptr;
}
m_offset = aligned + n;
return m_buffer + aligned;
```

---

## 2. Build Status

**Toolchain:** MSVC 19.51 (VS 2026 / VS 18 Enterprise) via `vcvars64.bat`  
**Preset:** manual configure — `cmake -S … -B … -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<vcpkg> -DVCPKG_TARGET_TRIPLET=x64-windows-static-md`  
**Target built:** `test_allocator` (+ `engine_demo` static library)

| Metric | Result |
|---|---|
| Compile errors | **0** |
| Linker errors (fix targets) | **0** |
| New warnings | **0** |

### Pre-existing build failure (not related to BUG-001)
`apps/sandbox/test-minimal.exe` fails to link due to missing `glfw3.lib` in its CMake link line — `raylib.lib` references `glfwInit` and ~70 other GLFW symbols that are not listed in the sandbox target's `target_link_libraries`. This failure is present on `main` before any BUG-001 changes and is outside this fix's scope.

---

## 3. Test Results

**Command:** `ctest --test-dir <build> --tests-regex test_allocator -V`  
**Suite:** `allocator` (3 tests)

| Test | Result |
|---|---|
| `allocator.basic_allocation_returns_aligned_pointer` | ✅ PASSED |
| `allocator.exhaustion_returns_null` | ✅ PASSED |
| `allocator.third_aligned_alloc_does_not_overrun_arena` *(BUG-001 regression)* | ✅ PASSED |

**3 / 3 passed. 0 failed. Total time: 0.95 s.**

The regression test `third_aligned_alloc_does_not_overrun_arena` directly exercises the crash scenario: three 40-byte, 32-byte-aligned allocations into a 160-byte arena. With the fix, the third call correctly returns `nullptr` and `bytes_used()` stays ≤ 160.

---

## 4. Constitution Compliance

| Rule | Status |
|---|---|
| No `std::` containers | ✅ — only `std::size_t`, `std::byte` (scalar types, not containers) |
| No `throw` / `try` / `catch` | ✅ |
| No `dynamic_cast` / `typeid` | ✅ |
| No raw `new` / `delete` | ✅ — bump-pointer arena, no heap allocation |
| `[[nodiscard]]` on `allocate()` | ✅ — confirmed in `allocator.h` |
| `noexcept` on moves/swaps | ✅ — `allocator(allocator&&)` and `operator=(allocator&&)` both `noexcept` |
| Explicit allocator parameter | ✅ — allocator is the backing store itself; no nested allocation |

---

## 5. Self-Test Checklist

- [x] Code compiles with zero errors in the worktree
- [x] No new warnings introduced
- [x] All allocator tests pass; pre-existing sandbox linker failure documented above
- [x] No `std::` containers, no exceptions, no RTTI
- [x] No raw `new`/`delete`, allocator pattern honored
- [x] Implementation notes written

---

**Handoff to:** `crash-qa`  
**Branch ready for QA:** `bugfix/BUG-001-null-write-destination`  
**Worktree path:** `.worktrees/bugfix-BUG-001-null-write-destination`
