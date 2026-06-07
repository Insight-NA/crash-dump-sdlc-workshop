# QA Verdict — BUG-001 / bugfix/BUG-001-null-write-destination

**Date:** 2026-06-07
**QA Agent:** crash-qa (mode)
**Branch:** bugfix/BUG-001-null-write-destination
**Worktree:** .worktrees/bugfix-BUG-001-null-write-destination
**Head commit:** c23b4b1 (regression test) / f66466f (allocator fix, on main)

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
| **test_regression_BUG_001 (6 tests)** | **6** | **0** | **0** |
| **TOTAL** | **16/16** | **0** | **5 DISABLED** |

ctest summary: **6/7** test targets pass (test_sandbox_render: Not Run, pre-existing).

---

## 2. Regression Test

**File:** tests/regression/BUG-001/test_regression_BUG_001.cpp
**CMake target:** test_regression_BUG_001
**Commit:** c23b4b1

| Test name | Purpose |
|---|---|
| aligned_padding_oob_returns_null_not_oob_ptr | H1: 96-byte arena, allocate(40,32)x2; pre-fix returns OOB ptr, post-fix returns nullptr |
| allocate_returned_ptr_satisfies_alignment | Alignment contract |
| returned_pointers_lie_within_arena_bounds | Bounds contract |
| zero_capacity_allocator_returns_null | Edge case |
| exhausted_arena_returns_null_and_caller_guards | H2: 128-byte arena, null-guard pattern |
| frame_budget_get_allocator_identity | H3: get_allocator() returns correct ref |

---

## 3. Regression Result

**Post-fix (c23b4b1):** 6/6 PASS ✅

**Pre-fix (bf83523 — parent of f66466f):** 1/6 FAIL ✅ (catches the bug)

Failure on pre-fix:
`
[ FAILED ] BUG_001_Regression.aligned_padding_oob_returns_null_not_oob_ptr
  Expected p2 == nullptr
  p2 is: 0000002CFD58EFA0 (OOB pointer, region [64,104) > 96-byte arena)
`

**Mechanism verified:** pre-fix llocator::allocate() checks m_offset + n > m_capacity
before lign_up(), so 40+40=80 <= 96 passes incorrectly. The aligned region
[64, 104) exceeds the 96-byte arena — exactly the OOB-write AV in BUG-001.dmp.

---

## 4. Pre-existing Failures (not regressions)

| Item | Classification |
|---|---|
| test_sandbox_render: Not Run | Pre-existing — glfw3.lib linker failure (no raylib in test env) |
| frame_budget.DISABLED_first_sample_is_not_double_counted_on_warmup | Pre-existing DISABLED (BUG-006 seeded defect) |
| game_loop.DISABLED_long_run_does_not_drift | Pre-existing DISABLED |
| rng.DISABLED_distinct_high_words_yield_distinct_streams | Pre-existing DISABLED |
| world.DISABLED_stale_handle_does_not_revive_after_recycle | Pre-existing DISABLED (BUG-003 seeded defect) |
| constraint_solver.DISABLED_solve_is_deterministic_across_construction_orders | Pre-existing DISABLED |
| D9025 warnings (/EHs override) | Pre-existing CMake flag ordering artifact |

---

## 5. Verdict: ✅ PASS

All active tests pass. Regression test demonstrates:
- Correct FAIL on pre-fix code (proves test catches BUG-001)
- Correct PASS on post-fix code
No pre-existing tests broken by this branch's changes.

---

## 6. Comparison (all branches)

| Rank | Branch | Active Tests | Verdict | Notes |
|---|---|---|---|---|
| 1 | bugfix/BUG-001-null-write-destination (H1) | 16/16 | **PASS** | Carries regression suite; fix is root-cause resolution |
| 2 | bugfix/BUG-001-unchecked-alloc-return (H2) | 10/10 | **PASS** | Caller null-guard + RAII arena |
| 3 | bugfix/BUG-001-deferred-init-member (H3) | 10/10 | **PASS** | Defensive hardening |
