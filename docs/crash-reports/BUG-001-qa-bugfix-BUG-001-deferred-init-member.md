# QA Verdict — BUG-001 / bugfix/BUG-001-deferred-init-member

**Date:** 2026-06-07
**QA Agent:** crash-qa (mode)
**Branch:** bugfix/BUG-001-deferred-init-member
**Worktree:** .worktrees/bugfix-BUG-001-deferred-init-member
**Head commit:** c196eef

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

H3 changes (from diff vs main):
- include/engine_demo/frame_budget.h: `allocator* m_alloc` -> `allocator* m_alloc{nullptr}`
- include/engine_demo/ecs/world.h: `allocator* m_alloc` -> `allocator* m_alloc{nullptr}`

These are latent-defect hardening changes; no existing code path exercises the
pre-fix undefined behaviour (indeterminate pointer on an unconstructed object).
The regression test `frame_budget_get_allocator_identity` (in the H1 regression
suite, test_regression_BUG_001) verifies the positive invariant: a correctly
constructed frame_budget returns the supplied allocator from get_allocator().

---

## 3. Regression Result

**Post-fix H3 (c196eef):** 10/10 active tests PASS ✅

**Pre-fix regression test verification (CONDITIONAL):** The {nullptr} default
init prevents indeterminate state in moved-from or incompletely-constructed
objects. No current test exercises such a path because the defect is latent —
the two-phase construction pattern is documented but never exercised in the test
suite. Therefore no regression test can be constructed that FAILS on pre-fix
headers and PASSES on post-fix without engineering a new unconstructed-object
scenario that itself would be a unit test for pre-existing UB behaviour.

The frame_budget_get_allocator_identity test (H1 regression suite) was verified
PASS on both pre-fix and post-fix headers, confirming the positive invariant is
unchanged. This is acceptable for a defensive-hardening change with no
exercisable pre-fix failure path.

---

## 4. Pre-existing Failures (not regressions)

| Item | Classification |
|---|---|
| test_sandbox_render: Not Run | Pre-existing — glfw3.lib linker failure |
| 5 DISABLED tests | Pre-existing seeded defects (BUG-003, BUG-006, others) |
| D9025 warnings | Pre-existing CMake flag ordering artifact |

---

## 5. Verdict: ✅ CONDITIONAL

All active tests pass. No pre-existing tests broken. H3 is a defensive
hardening change (explicit {nullptr} init on raw pointer members); the latent
defect it guards against has no exercisable pre-fix failure path in the current
test suite. Verdict is CONDITIONAL per mode rules: all existing tests pass and
the regression test cannot be verified to fail pre-fix by design.

For the QA self-test checklist:
- [x] Full test suite run — 10/10 active pass
- [x] Regression test exists (frame_budget_get_allocator_identity in H1 suite)
- [ ] Regression test verified to fail on pre-fix — NOT SATISFIABLE (latent defect, no exercisable path)
- [x] No pre-existing tests broken

---

## 6. Comparison (all branches)

| Rank | Branch | Active Tests | Verdict | Notes |
|---|---|---|---|---|
| 1 | bugfix/BUG-001-null-write-destination (H1) | 16/16 | **PASS** | Root-cause fix + verified regression suite |
| 2 | bugfix/BUG-001-unchecked-alloc-return (H2) | 10/10 | **PASS** | Caller null-guard + RAII arena |
| 3 | bugfix/BUG-001-deferred-init-member (H3) | 10/10 | **CONDITIONAL** | Hardening-only; latent defect, no exercisable pre-fix path |
