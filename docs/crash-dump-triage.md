# Demo pattern: Crash-dump triage

## Identity

- **id:** `crash-dump-triage`
- **drives:** Session 02 — Bug & Defect Fixing — Crash Dumps.
- **Copilot plan tier:** Business minimum.

## Learning objective

Given a minidump (`.dmp`), its matching symbols (`.pdb`), and the source revision tag, drive Copilot to:

1. Pinpoint the faulting frame (function + line) with explicit citations.
2. Propose a root cause supported by the disassembly + source.
3. Produce a minimal fix that the failing test newly passes against.
4. Re-run the failing test to confirm.

The win condition is **provenance-grounded**: every claim Copilot makes traces to a frame, a line, or a doc citation.

## Setup

- Demo workspace: the repo root, built via `cmake --preset default-debug` then `--build`.
- Fixture: `fixtures/crash_dumps/BUG-001/` containing `repro.cpp`, `capture.sh`/`capture.ps1`, and `README.md` (capture-it-yourself model — no `.dmp` is committed).
- Capture script: `fixtures/crash_dumps/BUG-001/capture.sh` (Linux/macOS) or `capture.ps1` (Windows) is rerun **the day before** the session to refresh the dump against locally-built symbols (symbol drift is the #1 demo failure).
- Required Copilot context for the demo: `#file:fixtures/crash_dumps/BUG-001/BUG-001.dmp.local` + `#file:src/engine_demo/allocator.cpp` + `#file:tests/engine_demo/test_allocator.cpp`.

## Facilitation script — Phase 1: pinpoint the faulting frame (15 min)

Switch Copilot to **Plan Mode**. Type verbatim:

> _"Read `#file:fixtures/crash_dumps/BUG-001/BUG-001.dmp.local`. Identify the faulting frame: function name, source file, line number. Cite the offset within the function. Do not propose fixes yet."_

**Expected:** Copilot returns frame `engine_demo::arena_allocator::allocate` at `src/engine_demo/allocator.cpp:LL`.

> **HITL gate:** facilitator confirms the frame against the actual disassembly before continuing. If Copilot mis-attributes, that's a teaching moment — show how to pull `windbg` / `lldb` output and feed it back as additional context.

## Facilitation script — Phase 2: root-cause analysis (20 min)

Type verbatim:

> _"Working from the faulting frame, trace the data flow that leads to the fault. Cite each source line you reference. Identify the precondition that was violated. Output as a numbered list of facts → conclusion."_

**Expected:** Copilot produces a 5–8 step trace ending in _"the bookkeeping pointer was advanced past the arena's end before the bounds check"_.

> **HITL gate:** facilitator pushes back on any unsupported claim. _"You said X — show me the line."_

## Facilitation script — Phase 3: fix + verify (20 min)

Switch to **Edit Mode**. Type verbatim:

> _"Propose the smallest fix that makes the failing test in `#file:tests/engine_demo/test_allocator.cpp` pass without violating `#file:.github/instructions/cpp-snippets.instructions.md`. Show the diff only; do not run anything yet."_

> **HITL gate:** facilitator reviews the diff. _"Does this fix introduce any new behavior? Does it satisfy the constitution?"_ If yes, run:

```text
ctest --preset default-debug --output-on-failure -R allocator
```

The previously failing test must pass; no other test must regress.

## Common pitfalls

1. **Stale PDB.** If the PDB does not match the source, every line attribution is wrong. The capture script enforces match — re-run it the day before.
2. **Copilot guesses without citing.** When that happens, restart with _"cite the line"_. Do not negotiate.
3. **Premature fix.** Skipping Phase 2 produces a "fix" that masks the bug. Always force the trace.
4. **Token budget exhaustion** on large allocators — pin only the relevant `src/engine_demo/allocator.cpp` and the test, not the whole subsystem.

## Recovery script

If Copilot produces a fix that uses `std::vector` or wraps the failing code in `try`/`catch`:

> _"Stop. The codebase compiles `-fno-exceptions -fno-rtti`. Re-do the fix using `eastl::expected<T, status>` and EASTL containers, per `#file:.github/instructions/cpp-snippets.instructions.md`."_

If Plan Mode loops without progressing past Phase 2:

> _"Switch to Edit Mode. Produce the smallest plausible fix as a diff. We'll evaluate it together."_

## Stretch goals

- Replay the fix as a `git revert` thought-experiment: ask Copilot to identify the commit that _introduced_ the bug. Don't actually revert.
- Have Copilot write a regression test that fails before the fix and passes after.

## Required Copilot plan tier

Business is sufficient. No Coding Agent or MCP servers required for this pattern.
