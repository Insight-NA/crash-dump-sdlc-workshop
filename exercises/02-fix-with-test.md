# Exercise 02 — Fix BUG-001 with a regression test

**Time:** 15 min · **Difficulty:** intermediate

## Goal

Produce your own fix + test for BUG-001 such that `ctest --preset default-debug --output-on-failure` is green and the previously-disabled regression test runs.

## Steps

1. Switch Copilot Chat to **Edit Mode**.
2. Apply the Phase-3 prompt from the learner guide.
3. Inspect the diff. Specifically check that:
   - No exceptions are thrown.
   - No RTTI-dependent constructs are introduced.
   - The function still returns `nullptr` on overflow (the contract).
4. Run `ctest --preset default-debug --output-on-failure`.

## Acceptance

- All tests pass, including `allocator.third_aligned_alloc_does_not_overrun_arena` (no longer DISABLED).
- The reviewer chat mode reports zero Critical findings on the diff.

## Stretch

Add a property-style test that constructs random valid (size, alignment) pairs and asserts the allocator either returns a valid aligned pointer **or** `nullptr`, but never advances `m_offset` past `m_capacity`.
