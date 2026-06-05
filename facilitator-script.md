# Session 02 — Bug & Defect Fixing: Crash Dumps (facilitator script)

> **Cross-references:** [`learner-guide.md`](learner-guide.md) · [`slides.outline.md`](slides.outline.md) · [`retro-survey.md`](retro-survey.md) · [exercises](exercises/)

## Session at a glance

- **Duration:** 60 min · **Level:** advanced · **Plan tier:** Business
- **Demo workspace state:** `fixtures/crash_dumps/BUG-001/` populated; `BUG-001.dmp` (or `.core`) captured locally beforehand.
- **Outcomes:**
  1. Walk a `.dmp` from "stack frame" → "root cause" using Copilot in <15 minutes.
  2. Apply the **3-phase crash triage** pattern (pinpoint → root-cause → fix-with-test).
  3. Recognize when Copilot's first hypothesis is wrong and how to redirect it.

## Pre-session checklist (T-15 min)

> **Do:** confirm `BUG-001.dmp.local` (or `.core.local`) exists; if not, run `fixtures/crash_dumps/BUG-001/capture.sh` (Linux) or `capture.ps1` (Windows).
> **Do:** open `src/engine_demo/allocator.cpp` and `include/engine_demo/allocator.h` side by side.
> **Do:** load `factory-overlay/demo-patterns/crash-dump-triage.md` in a third tab as facilitator reference.
> **Watch for:** the dump _must_ match the locally-built `repro.exe`/`./repro` symbols. If symbols are stale, re-capture.

## Opening (0:00 – 0:05)

> **Say:** "Last week we built context belts for unfamiliar code. This week we apply that muscle to the worst possible context: a crash dump from production. You will leave knowing a 3-phase pattern that works on every dump you'll ever see."

> **Do:** show the agenda. Three phases: pinpoint (15 min), root-cause (20 min), fix-with-test (20 min).

## Block 1: Phase 1 — pinpoint the failing frame (0:05 – 0:20)

> **Do:** open `BUG-001.dmp.local` in VS Code's debug view (Linux: `gdb ./repro core`; Windows: WinDbg or VS).

> **Do:** in Plan Mode, attach the dump-adjacent files and ask:

```text
#file:fixtures/crash_dumps/BUG-001/repro.cpp
#file:include/engine_demo/allocator.h
#file:src/engine_demo/allocator.cpp
A debugger reports a heap-corruption fault inside the third call to allocator::allocate.
Identify the call stack frame most likely responsible for the corruption. Cite the line.
```

> **Watch for:** Copilot pinpoints the bookkeeping write inside `allocate()`. If it instead points at the caller, redirect: "The crash is _inside_ allocator::allocate, not in the caller. Re-examine."

> **HITL gate:** before moving to Phase 2, confirm participants identified the correct frame. Show of hands. If <70% got it, recap.

## Block 2: Phase 2 — root cause (0:20 – 0:40)

> **Say:** "Phase 2 is data flow. We trace the _write_ that corrupted the heap back through the function, asking Copilot to enumerate every state mutation."

```text
#file:src/engine_demo/allocator.cpp
List every line that mutates m_offset, in source order. For each, state the precondition
that must hold for the mutation to be safe.
```

> **Watch for:** Copilot identifies that `m_offset = align_up(...)` runs _before_ the bounds check on `end`. That ordering is the bug.

> **Do:** ask the follow-up:

```text
What invariant does the current order break? State it as one sentence.
```

> **Watch for:** "the arena offset must not advance past the buffer tail" or equivalent. If Copilot waffles, narrow with: "What is the maximum legal value of m_offset?"

## Block 3: Phase 3 — fix with a regression test (0:40 – 0:55)

> **Say:** "Now we move to Edit Mode. The fix and the regression test are the same task — never one without the other."

> **Do:** switch to Edit Mode.

```text
#file:src/engine_demo/allocator.cpp
#file:tests/engine_demo/test_allocator.cpp
Refactor allocate() so the bounds check happens BEFORE m_offset is mutated. Re-enable the
DISABLED_third_aligned_alloc_does_not_overrun_arena test (drop the DISABLED_ prefix).
Constitutional articles 1, 2 must still hold.
```

> **HITL gate:** before accepting the diff, the facilitator reads it aloud. The fix must compute aligned offset and end into local variables, validate against capacity, then commit to `m_offset`.

> **Do:** invoke the `reviewer` chat mode on the diff:

```text
Use the reviewer chat mode. Severity-rate the proposed diff.
```

> **Watch for:** zero Critical findings. If Significant findings appear, the facilitator decides on the spot whether to accept or iterate (this is the moment learners see HITL in action).

## Q&A (0:55 – 0:58)

- **Q: "What if we don't have symbols?"** → "Plan B: capture the address, ask Copilot to enumerate plausible candidates from the source. It's slower but works."
- **Q: "Multi-threaded crashes?"** → Session 04 covers that briefly; full treatment in Session 09 (squad pattern).

## Retro micro-survey (0:58 – 1:00)

> **Do:** post [`retro-survey.md`](retro-survey.md). Read aloud Q1 + Q5.

## Stretch goals

- Run the same flow on BUG-006 (off-by-one) with a contrived crash from an out-of-window read.
- Compare Plan-Mode-only diagnosis vs. Edit-Mode-driven diagnosis.

## Recovery script (if a demo fails)

If the dump won't load (mismatched symbols):

> **Do:** fall back to a static walkthrough using `fixtures/crash_dumps/BUG-001/README.md` and the source. The 3-phase pattern still applies — only the debugger's role is replaced by manual reading.

If Copilot pinpoints the wrong frame:

> **Say:** "This is a teachable moment — Copilot is wrong roughly 1 in 5 dumps. Watch how I redirect it."

## Post-session checklist (T+15 min)

- Upload recording.
- Capture the diff produced live as a gist for the learner guide's "what we did on stage" link.
- Export retro responses; cross-reference Q5 against Session 03 plan.
