# Session 02 — Crash Dumps (learner guide)

## Before you start

- VS Code + Copilot Chat (Business plan).
- Local debug build of the demo workspace: `cmake --preset default-debug && cmake --build --preset default-debug` from the repo root.
- Crash dump captured: run `fixtures/crash_dumps/BUG-001/capture.sh` (Linux) or `capture.ps1` (Windows). The dump lands as `BUG-001.dmp.local` (or `.core.local`).

## Outcomes

After this session:

1. You can apply the 3-phase triage pattern (pinpoint → root cause → fix-with-test) to a real crash dump.
2. You know how to redirect Copilot when it pinpoints the wrong frame.
3. You understand why the fix and the regression test are one task, not two.

## Walk-through

### Phase 1 — pinpoint

Open the dump in VS Code's debug view. In Plan Mode:

```text
#file:fixtures/crash_dumps/BUG-001/repro.cpp
#file:include/engine_demo/allocator.h
#file:src/engine_demo/allocator.cpp
A debugger reports a heap-corruption fault inside the third call to allocator::allocate.
Identify the call stack frame most likely responsible for the corruption. Cite the line.
```

Expected: Copilot identifies the bookkeeping write inside `allocate()`.

### Phase 2 — root cause

```text
#file:src/engine_demo/allocator.cpp
List every line that mutates m_offset, in source order. For each, state the precondition
that must hold for the mutation to be safe.
```

Expected: enumeration that exposes the bounds check happening _after_ the offset advances.

### Phase 3 — fix-with-test

Switch to Edit Mode. Apply this prompt:

```text
#file:src/engine_demo/allocator.cpp
#file:tests/engine_demo/test_allocator.cpp
Refactor allocate() so the bounds check happens BEFORE m_offset is mutated. Re-enable the
DISABLED_third_aligned_alloc_does_not_overrun_arena test (drop the DISABLED_ prefix).
Constitutional articles 1, 2 must still hold.
```

Run `ctest --preset default-debug --output-on-failure`. The previously-disabled regression should pass.

## Try it yourself

### Exercise 1 — re-walk Phase 1 unaided

**Goal:** open `BUG-001.dmp` cold (no script) and find the failing frame in <5 minutes. See [exercises/01-pinpoint.md](exercises/01-pinpoint.md).

### Exercise 2 — fix BUG-001 yourself

**Goal:** without copying the live demo's diff, produce your own fix + regression test that ctest accepts. See [exercises/02-fix-with-test.md](exercises/02-fix-with-test.md).

## Troubleshooting

| Symptom                                   | Fix                                                               |
| ----------------------------------------- | ----------------------------------------------------------------- |
| Dump won't load (mismatched symbols)      | Re-capture: rebuild then re-run `capture.sh` / `capture.ps1`.     |
| Copilot blames the caller, not allocate() | Redirect: "the crash is _inside_ allocator::allocate".            |
| ctest still passes after fix              | You forgot the DISABLED\_ prefix. Strip it; re-run.               |
| Fix breaks `noexcept` contract            | Re-read constitutional Article 1 — no exceptions; status returns. |

## Going further

- Read [`docs/crash-dump-triage.md`](docs/crash-dump-triage.md) for the canonical 3-phase pattern.
- Apply the pattern to one of your real production dumps — bring findings to office hours.

## Feedback

[`retro-survey.md`](retro-survey.md). 60 seconds.
