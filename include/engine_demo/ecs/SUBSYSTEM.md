# engine_demo/ecs subsystem

> **Anchor file** for Sessions 01 and 05.

The ECS world is a slot-based, generational entity store with O(1) create/destroy and O(1)
liveness queries. It is the canonical example used in Session 01 (Code Comprehension at
Scale) and Session 05 (Context Engineering at Scale).

## Layout

```text
include/engine_demo/ecs/
  world.h                 # entity_handle, world (this file documents)
  components/
    transform.h           # (added per spec-kit feature)
    health.h              # (added per spec-kit feature)
src/engine_demo/ecs/
  world.cpp               # implementation; contains SEEDED DEFECT BUG-003
tests/engine_demo/
  test_world.cpp
```

## Constitutional articles touched

- **3 (EASTL-first)** — internal storage uses EASTL containers only.
- **4 (allocator-aware)** — all containers take an explicit allocator at construction.
- **6 (real-time)** — create / destroy are O(1) amortized; iteration is cache-friendly.

## Seeded defect

`destroy_entity` in `src/engine_demo/ecs/world.cpp` does **not** bump the slot generation
counter on free, allowing a stale handle to validate against a recycled slot
(use-after-free / CWE-416). Session 04 fixes it on stage.

## Code-comprehension exercise (Session 01)

1. Open `include/engine_demo/ecs/world.h` in Plan Mode.
2. Ask Copilot to summarize the entity-handle invariant in one sentence.
3. Ask Copilot to enumerate every code path that mutates `slot.generation`. (There is
   currently exactly one — and that's the bug.)
4. Switch to Edit Mode only after the summary matches your manual reading.

## Context-engineering exercise (Session 05)

Construct a context belt that includes:

- This `SUBSYSTEM.md` (subsystem charter).
- `include/engine_demo/ecs/world.h` (interface).
- `specs/constitution.md` (ground truth).
- A pinned chat-mode reference to `cpp-architect.chatmode.md`.

Then ask Copilot to extend the world with a `component_storage<T>` template, observing
which articles of the constitution it cites in its plan.
