# Lockless ring buffer — Session 06 demo feature

> **Stage:** skeleton only. The Session 06 facilitator runs `/specify`, `/plan`, `/tasks`,
> `/implement` live; this directory exists so the spec-kit CLI has a destination.

## Brief (one-paragraph problem statement)

Provide a single-producer / single-consumer lockless ring buffer for cross-thread message
hand-off in `engine_demo`. Capacity is fixed at construction; storage comes from an
`engine_demo::allocator&`. Backpressure policy is **drop-oldest** when full (configurable
in Session 07's stretch goal). Must satisfy constitutional articles 1–6 (no exceptions, no
RTTI, EASTL containers, allocator-aware, deterministic ordering, real-time / no
allocation in inner loops).

## Demo arc

1. `/constitution` — facilitator reviews the eight articles aloud (5 min).
2. `/specify` — capture the brief above as `spec.md` (10 min).
3. `/plan` — Copilot proposes a memory layout, atomics protocol, and tests-first ordering
   (10 min).
4. `/tasks` — Copilot decomposes into 4–6 task entries (10 min).
5. `/implement` — facilitator approves one task at a time; HITL between each (15 min).
