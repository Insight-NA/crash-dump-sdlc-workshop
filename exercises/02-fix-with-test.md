# Exercise 02 — Fix BUG-001 with a regression test

**Time:** 15 min · **Difficulty:** intermediate

## Goal

Produce your own fix + test for BUG-001 such that `ctest --preset default-debug --output-on-failure` is green and the previously-disabled regression test runs.

**The defect:** `allocator::allocate()` in `src/engine_demo/allocator.cpp` performs its
bounds check using the raw `m_offset` _before_ calling `align_up()`. Alignment padding can
push the actual end-address past the buffer tail, returning an out-of-bounds pointer without
a null check catching it — the root cause of the WRITE AV crash in `BUG-001.dmp`.

**The fix:** compute the aligned offset and the resulting end-address in locals _first_,
validate both against `m_capacity`, and only commit to `m_offset` after the check passes.

> **Builds on Exercise 01.** If you skipped [01-pinpoint.md](01-pinpoint.md), read the
> Phase 2 root-cause output there before continuing — it identifies the exact line to change.

## Steps

1. Switch Copilot Chat to **Edit mode**.

2. Add these two files to your context belt, then paste the prompt below:

   ```text
   #file:src/engine_demo/allocator.cpp
   #file:tests/engine_demo/test_allocator.cpp
   Refactor allocate() so the bounds check happens BEFORE m_offset is mutated. Compute the
   aligned offset and end in locals, validate against capacity, return nullptr on overflow,
   THEN commit to m_offset. Re-enable the DISABLED_third_aligned_alloc_does_not_overrun_arena
   test (drop the DISABLED_ prefix). Constitutional articles 1 (no exceptions) and 2 (no RTTI)
   must still hold — return a status/nullptr, never throw.
   ```

3. Inspect the diff before accepting. Verify:
   - `align_up(m_offset, alignment)` is stored in a local before the bounds check.
   - The bounds check compares `aligned + n > m_capacity` (not `m_offset + n`).
   - `m_offset` is only mutated after the check passes.
   - No `try`, `catch`, `throw`, or `dynamic_cast` appears anywhere in the diff.
   - The function still returns `nullptr` on overflow — the contract is preserved.

4. Run `ctest --preset default-debug --output-on-failure`.

## Acceptance

- All tests pass, including `allocator.third_aligned_alloc_does_not_overrun_arena` (no longer DISABLED).
- Manual compliance check on the diff:
  - `align_up` is called _before_ the bounds comparison (Article 1 — no exceptions; status/nullptr path).
  - No `dynamic_cast` or `typeid` introduced (Article 2 — no RTTI).
  - `m_offset` is only mutated after the guard confirms the allocation fits.

## Stretch

Add a property-style test that constructs random valid (size, alignment) pairs and asserts
the allocator either returns a valid aligned pointer **or** `nullptr`, but never lets
`bytes_used()` exceed `capacity()`. Use `eastl::mt19937` seeded explicitly (Constitution
Article 5 — determinism); never `std::random_device`.
