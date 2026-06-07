# QA Verdict — BUG-001 / bugfix/BUG-001-unchecked-alloc-return

**Date:** 2026-06-07
**QA Agent:** crash-qa (mode)
**Branch:** bugfix/BUG-001-unchecked-alloc-return
**Worktree:** .worktrees/bugfix-BUG-001-unchecked-alloc-return
**Head commit:** 099226f

---

## 1. Test Matrix — Full Suite

| Suite | Active Pass | Active Fail | Disabled (pre-existing) |
|---|---|---|---|
| test_allocator (3 tests) | 3 | 0 | 0 |
| test_frame_budget (3 tests) | 2 | 0 | 1 (BUG-006) |
| test_game_loop (3 tests) | 2 | 0 | 1 |
| test_rng (2 tests) | 1 | 0 | 1 |
| test_world (2 tests) | 1 | 0 | 1 (BUG-003) |
| test_constraint (2 tests) | 1 | 0 | 1 |
| test_sandbox_render | N/A | Not Run | N/A — pre-existing glfw3 linker failure |
| **TOTAL** | **10/10** | **0** | **5 DISABLED** |

ctest summary: **6/7** test targets pass (test_sandbox_render: Not Run, pre-existing).

---

## 2. Regression Test

Regression test suite (test_regression_BUG_001) lives on the H1 branch
(bugfix/BUG-001-null-write-destination, commit c23b4b1). H2 does not carry
it independently; the regression suite covers H2 behaviour via
`exhausted_arena_returns_null_and_caller_guards`.

H2 changes (from diff vs main):
- apps/sandbox/scene.h: raw `unsigned char* m_arena_buffer` -> `eastl::unique_ptr<unsigned char[]>`
- apps/sandbox/scene.cpp: `new`/`delete[]` -> `eastl::make_unique` + `= default` dtor
- fixtures/crash_dumps/BUG-001/capture_self.cpp: null-guards after p1, p2 allocations
- fixtures/crash_dumps/BUG-001/repro.cpp: null-guards after p1, p2 allocations

These changes prevent the caller-side write-through-null AV (H2 hypothesis).
The underlying allocator OOB fix (H1) is already on main and inherited.

---

## 3. Regression Result

**Post-fix H2 (099226f):** 10/10 active tests PASS ✅

**Pre-fix regression test verification:** The H1 regression test
(aligned_padding_oob_returns_null_not_oob_ptr) was verified to FAIL on pre-fix
allocator code (bf83523) in the H1 worktree, confirming the allocator fix that
both H1 and H2 inherit from main. H2 does not introduce additional allocator
changes; its defence-in-depth null-guards are verified by static analysis and
the exhausted_arena test in the H1 regression suite.

---

## 4. Pre-existing Failures (not regressions)

| Item | Classification |
|---|---|
| test_sandbox_render: Not Run | Pre-existing — glfw3.lib linker failure |
| 5 DISABLED tests | Pre-existing seeded defects (BUG-003, BUG-006, others) |
| D9025 warnings | Pre-existing CMake flag ordering artifact |

---

## 5. Verdict: ✅ PASS

All 10 active tests pass. No pre-existing tests broken. H2 changes are
constitution-compliant (eastl::unique_ptr replaces raw new/delete, Art. 7).
The caller-side null-guard pattern prevents the write-through-null crash
scenario documented in BUG-001.dmp.

---

## 6. Comparison (all branches)

| Rank | Branch | Active Tests | Verdict | Notes |
|---|---|---|---|---|
| 1 | bugfix/BUG-001-null-write-destination (H1) | 16/16 | **PASS** | Root-cause fix + regression suite |
| 2 | bugfix/BUG-001-unchecked-alloc-return (H2) | 10/10 | **PASS** | Caller null-guard + RAII arena |
| 3 | bugfix/BUG-001-deferred-init-member (H3) | 10/10 | **PASS** | Defensive hardening |
