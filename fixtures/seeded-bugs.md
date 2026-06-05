# Seeded bugs — engine_demo

> Six intentionally seeded defects across allocator, sim, ECS, frame budget, and physics.
> **Do not fix.** They are the demo material for Sessions 2, 3, and 4.

| ID       | Subsystem    | File:line (approx)                          | Class           | Session  | Notes                                                       |
| -------- | ------------ | ------------------------------------------- | --------------- | -------- | ----------------------------------------------------------- |
| BUG-001  | allocator    | `src/engine_demo/allocator.cpp:LL`          | OOB write       | 02       | Arena bookkeeping pointer advances past end before bounds check (CWE-787). Crashes on the third allocation of a particular size class. Fixture has matching `.dmp`/`.pdb`. |
| BUG-002  | sim          | `src/engine_demo/sim/game_loop.cpp:LL`      | Logic / drift   | 03       | Accumulator uses `float`; small `dt` values lose precision. Replay diverges after ~30s. |
| BUG-003  | ecs          | `src/engine_demo/ecs/world.cpp:LL`          | UAF / security  | 04 (sec) | Entity handle reused without generation bump; stale handle dereferences a freed slot (CWE-416). Stretch in S2; primary in S4. |
| BUG-004  | physics      | `src/engine_demo/physics/constraint.cpp:LL` | Non-determinism | 04       | Constraint-solver iteration order depends on hash-map traversal; replay diverges. |
| BUG-005  | sim          | `src/engine_demo/sim/rng.cpp:LL`            | Seed downcast   | 04 (sec) | `uint64_t` seed downcast to `uint32_t` when forwarding to `mt19937` constructor (CWE-197). |
| BUG-006  | frame budget | `src/engine_demo/frame_budget.cpp:LL`       | Off-by-one      | 03       | Rolling window advances index after read instead of before; first frame's sample is double-counted. |

## Triage rubric

For each candidate Copilot surfaces:

1. **Reproduce.** A failing test in <5 lines.
2. **Analyze data flow.** Trace input → defect.
3. **Decide.** Real bug → file an issue. False positive → one-line code comment.

## False-positive seeds (Session 4 only)

The Session 4 demo run is salted with two false positives Copilot is likely to flag:

- **FP-001:** the seed-derivation function appears to truncate but actually re-extends; the
  truncation is a narrow widen, not a downcast.
- **FP-002:** the constraint-solver loop appears to read past the end, but the trailing
  iteration is a sentinel that never reads the buffer.

Learners must use the rubric to dismiss these, with a one-line comment near the suspect
line.
