# Session 02 — Crash Dumps: Self-Guided Learner Guide

> **You can do this whole session alone.** Every activity below is a prompt you paste
> into GitHub Copilot Chat. Work top to bottom. The agent explains each step, you run it,
> then you move to the next one.

## How to use this guide

1. Open this repo in VS Code with GitHub Copilot Chat enabled (Business/Enterprise plan).
2. Start with **Section 1 — Setup & Verification**. Don't skip it; it confirms your machine
   and the MCP servers are ready.
3. Pick a track:
   - **Track A — Manual Triage** (Section 3): source-based crash reasoning in Plan/Edit mode.
     Runs on **macOS, Linux, and Windows**. Start here.
   - **Track B — Agentic Pipeline** (Section 4): the full 6-agent orchestrated flow on a live
     `.dmp`. Requires **Windows + WinDbg (`cdb.exe`)**.
4. Want to be taught step by step? Use the **Teacher Mode** prompt in Section 2 first — it
   turns the agent into an instructor that walks you through each activity and waits for you.

### Which track can I run?

| You're on…                  | Track A (manual)  | Track B (agentic pipeline) |
| --------------------------- | ----------------- | -------------------------- |
| macOS / Linux               | ✅ Yes            | ❌ No (needs Windows cdb)  |
| Windows + WinDbg installed  | ✅ Yes            | ✅ Yes                     |
| Windows, no WinDbg          | ✅ Yes            | ❌ Install WinDbg first    |

## Outcomes

After this session you can:

1. Apply the 3-phase triage pattern (pinpoint → root cause → fix-with-test) to a real crash.
2. Redirect Copilot when it pinpoints the wrong frame.
3. Explain why the fix and its regression test are one task, not two.
4. Drive a multi-agent crash-to-fix pipeline through its 3 human-in-the-loop gates.
5. Keep premium-request (token) usage low while getting deeper agent reasoning.

---

## 1. Setup & Verification

### 1.1 Build the demo (terminal)

From the repo root:

```bash
cmake --preset default-debug
cmake --build --preset default-debug
ctest --preset default-debug --output-on-failure
```

> One disabled test (`DISABLED_third_aligned_alloc_does_not_overrun_arena`) is expected to be
> skipped — that's the seeded bug you'll fix later.

### 1.2 Run the Setup Verification Prompt

Open Copilot Chat in **Agent mode** and paste this. It checks your toolchain, builds, runs
tests, and confirms both MCP servers are live before you invest time in the exercises.

```text
You are my environment verifier for this C++ game-engine workshop. Run these checks and
report a PASS/FAIL table with one fix suggestion per FAIL. Do NOT modify any source files.

1. Toolchain: confirm cmake (>=3.28), ninja, and a C++20 compiler are on PATH. Confirm
   $VCPKG_ROOT is set. Confirm node (>=20) and npx are available (needed for the MCP servers).
2. Build: run `cmake --preset default-debug` then `cmake --build --preset default-debug`.
   Report errors only.
3. Tests: run `ctest --preset default-debug --output-on-failure`. Report pass/fail counts.
   It is EXPECTED that DISABLED_third_aligned_alloc_does_not_overrun_arena is skipped.
4. MCP servers: read .vscode/mcp.json. Confirm two servers are registered: crash-dump-mcp
   and worktree-mcp. Then list the MCP tools you currently have available and tell me whether
   tools prefixed worktree-mcp/ (e.g. create_worktree, cmake_build, run_tests) and
   crash-dump-mcp/ (e.g. parse_minidump, get_call_stack, analyze_crash) are loaded.
5. Track readiness: tell me if I can run Track B. Track B needs Windows + a valid CDB_PATH
   (cdb.exe from WinDbg). If I'm on macOS/Linux, say "Track A only" and explain why.

End with: "Ready for Track A" and/or "Ready for Track B".
```

**If the MCP tools don't appear:** open the Command Palette → **MCP: List Servers**, then
start `crash-dump-mcp` and `worktree-mcp`. They auto-start on workspace open, but a fresh
clone needs `npx tsx` available (Node ≥ 20). See Troubleshooting (Section 6).

---

## 2. Teacher Mode (optional but recommended for solo learners)

Paste this **once** at the start of a session. It turns the agent into an instructor that
describes each activity, tells you exactly what to paste next, then **stops and waits** for
you to report results before moving on. This is how you "step through the training yourself"
and still get taught.

```text
Be my workshop instructor for Session 02 (Crash Dumps) in this repo. Teach me one step at a
time using the learner-guide.md flow. For EACH step:
1. In 2-3 sentences, explain what the step teaches and why it matters.
2. Give me the exact prompt or command to run next, in a single copy-paste block.
3. STOP. Wait for me to paste back the result or say "next". Do not continue on your own.
When I report a result, briefly tell me if it looks right, what to notice, then advance to
the next step. Start with Track A, Phase 1 (pinpoint). Keep your explanations short to save
tokens. Begin now with Step 1.
```

> The agent will pause after each step. Reply with what you saw (or just `next`) to continue.

---

## 3. Track A — Manual Triage (runs everywhere)

The bug: in [src/engine_demo/allocator.cpp](src/engine_demo/allocator.cpp) the arena bounds
check runs **after** `m_offset` advances, so alignment padding can push an allocation past the
buffer tail. The third 40-byte allocation at 32-byte alignment overruns and crashes.

### Phase 1 — Pinpoint (Plan mode)

Switch Copilot Chat to **Plan mode**, then paste:

```text
#file:fixtures/crash_dumps/BUG-001/repro.cpp
#file:include/engine_demo/allocator.h
#file:src/engine_demo/allocator.cpp
A debugger reports a heap-corruption fault inside the third call to allocator::allocate.
Identify the call stack frame most likely responsible for the corruption. Cite the line.
```

**Expected:** Copilot points at the bookkeeping write inside `allocate()` where `m_offset`
advances before the bounds check.

> **If Copilot blames the caller, not `allocate()`:** redirect with
> `The crash is INSIDE allocator::allocate, not the caller. Re-pinpoint the exact line.`

### Phase 2 — Root cause (Plan mode)

```text
#file:src/engine_demo/allocator.cpp
List every line that mutates m_offset, in source order. For each, state the precondition
that must hold for the mutation to be safe.
```

**Expected:** an enumeration that exposes the bounds check happening *after* the offset
advances — the precondition for the unsafe write is never validated.

### Phase 3 — Fix with test (Edit/Agent mode)

Switch to **Edit mode** (or Agent mode) and paste:

```text
#file:src/engine_demo/allocator.cpp
#file:tests/engine_demo/test_allocator.cpp
Refactor allocate() so the bounds check happens BEFORE m_offset is mutated. Compute the
aligned offset and end in locals, validate against capacity, return nullptr on overflow,
THEN commit to m_offset. Re-enable the DISABLED_third_aligned_alloc_does_not_overrun_arena
test (drop the DISABLED_ prefix). Constitutional articles 1 (no exceptions) and 2 (no RTTI)
must still hold — return a status/nullptr, never throw.
```

Then verify:

```bash
ctest --preset default-debug --output-on-failure
```

**Expected:** the previously-disabled regression test now passes, and nothing else breaks.

> **Why fix + test together?** A fix with no test can silently regress later. The regression
> test *is* the proof the fix works — they're one unit of work, not two.

---

## 4. Track B — Agentic Pipeline (Windows + WinDbg)

This track runs the full 6-agent crash-to-fix pipeline on the live dump. The
`crash-dump-mcp` server wraps Windows `cdb.exe`, so **this section requires Windows with
WinDbg installed and `CDB_PATH` set in `.vscode/mcp.json`**. On macOS/Linux, read it as
reference and stick to Track A.

### 4.1 Capture the dump (once)

```powershell
cd fixtures\crash_dumps\BUG-001
.\capture.ps1
```

This produces `BUG-001.dmp` next to the reproducer.

### 4.2 The 6 agents and what they produce

| Agent                  | Does                                                   | Writes                    |
| ---------------------- | ------------------------------------------------------ | ------------------------- |
| `crash-orchestrator`   | Drives the pipeline, enforces the 3 HITL gates         | `BUG-001-final.md`        |
| `crash-analyzer`       | Parses dump, 3 root-cause hypotheses (tree-of-thought) | `BUG-001-analysis.md`     |
| `crash-planner`        | 3 fix strategies + 3 git worktrees                     | `BUG-001-plan.md`         |
| `crash-engineer`       | Implements the minimum viable fix per branch           | `BUG-001-impl-{a,b,c}.md` |
| `crash-qa`             | Runs tests, generates regression test per branch       | `BUG-001-qa-{a,b,c}.md`   |
| `crash-validator`      | Scores branches, recommends the winner                 | `BUG-001-resolution.md`   |

All artifacts land in [docs/crash-reports/](docs/crash-reports/).

### 4.3 Step 1 — Kick off the pipeline

Select the **`crash-orchestrator`** agent in Copilot Chat, then paste:

```text
I have a crash dump at fixtures/crash_dumps/BUG-001/BUG-001.dmp with symbols at
fixtures/crash_dumps/BUG-001/. Bug ID is BUG-001. Run the crash-to-fix pipeline: analyze,
plan 3 parallel fixes in worktrees, implement, QA each, validate, and pause at every HITL
gate for my decision. Begin with intake and analysis.
```

The orchestrator delegates to `crash-analyzer`, which calls `parse_minidump` →
`get_call_stack` → `analyze_crash` and writes the analysis.

### 4.4 HITL Gate #1 — Approve the analysis

When the orchestrator presents 3 hypotheses and pauses, reply:

```text
APPROVE. Hypothesis A (bounds check ordering) looks correct. Proceed to planning, and let
all 3 branches run so I can compare approaches.
```

### 4.5 HITL Gate #2 — Approve the plan

After `crash-planner` creates 3 worktrees and a strategy per branch, reply:

```text
APPROVE the plan. All 3 strategies are reasonable. Proceed to implementation and QA on every
branch. Each fix must follow the constitution: no exceptions, no RTTI, EASTL-only,
allocator-aware, and add the regression test.
```

`crash-engineer` implements in each worktree; `crash-qa` runs `cmake_build` + `run_tests`
and generates the regression test; `crash-validator` scores them.

### 4.6 HITL Gate #3 — Select and merge

When `crash-validator` presents the scored comparison and recommends a branch, reply:

```text
MERGE the recommended branch (the local-variable ordering fix). Then remove all worktrees,
delete the orphaned branches, and write the final audit trail to
docs/crash-reports/BUG-001-final.md with timeline, decisions, and the merge commit SHA.
```

**Done:** you've taken a raw dump to a tested, merged fix with a full audit trail — and you
made the call at all three gates.

---

## 5. GitHub Copilot Token Usage — and how to save it

### 5.1 How you're billed

GitHub Copilot Chat (ask, edit, **agent**, and plan modes) consumes **one premium request
per user prompt**, multiplied by the model's multiplier. The key insight for this workshop:

> **The tool calls the agent makes on its own are free.** Only the prompts *you* send count.
> One agent prompt that triggers ten `parse_minidump` / `cmake_build` / `run_tests` calls is
> still **one** premium request.

That changes how you should prompt: **fewer, richer prompts beat many small ones.**

| Concept                | What it means for you                                                |
| ---------------------- | -------------------------------------------------------------------- |
| 1 prompt = 1 request   | Each message you send = one premium request × model multiplier.      |
| Tool calls are free    | The agent's autonomous MCP/build/test calls don't add requests.      |
| Model multiplier       | Heavier models cost more per request; lighter models cost less/none. |
| Monthly allowance      | Resets on the 1st (UTC); unused requests don't roll over.            |

Source: GitHub Docs — *Requests in GitHub Copilot* (Copilot billing → premium requests).

### 5.2 Spend fewer requests

- **Batch multi-part asks into one prompt.** Instead of three messages ("list the lines",
  "now the preconditions", "now the fix"), ask for all three in one structured prompt.
- **Let the agent finish the chain.** In Track B, one orchestrator prompt runs the whole
  analysis. Don't nudge it with extra messages unless it stalls — each nudge is a new request.
- **Use the right mode.** Use **Plan/Ask** mode for read-only reasoning (pinpoint, root
  cause). Only switch to **Edit/Agent** when you actually want file changes.
- **Pick a lighter model for routine steps.** Reserve high-multiplier models for the hard
  analysis (Phase 1/2, Gate #1). Use a lower-multiplier or included model for mechanical
  steps (re-running tests, formatting an artifact).
- **Avoid Copilot code review for trivial diffs** — each IDE/PR review consumes 13 requests.

### 5.3 Spend less context (cheaper, sharper answers)

- **Tight context belts.** Reference only the files that matter with `#file:` — the 3-file
  belt in Phase 1, not the whole `src/` tree. Less context = faster, more focused reasoning.
- **Cite the constraint, don't re-explain it.** Say "Constitution Article 1 (no exceptions)"
  instead of pasting the rule — the agent already has the instructions loaded.
- **Start fresh threads per phase.** A long thread re-sends its whole history as context.
  New phase → new chat keeps each request lean.

### 5.4 Maximize agent thinking (more reasoning, same request)

Because tool calls are free, push the work *into* a single prompt:

- **Front-load context and goal.** State the files, the symptom, and the exact deliverable up
  front so the agent doesn't burn a round-trip clarifying.
- **Ask for explicit reasoning structure.** "List every line that mutates `m_offset`, in
  source order, with the precondition for each" yields deeper analysis than "what's wrong?".
- **Demand evidence.** "Cite the file and line" / "show the failing test output" forces the
  agent to ground its answer instead of guessing.
- **Let it run the full tool chain.** Tell it to build, test, and report — all inside one
  prompt — rather than you driving each tool with a separate message.

---

## 6. Troubleshooting

- **MCP tools missing in chat** — Command Palette → **MCP: List Servers** → start
  `crash-dump-mcp` / `worktree-mcp`.
- **MCP server won't start** — Ensure Node ≥ 20 and `npx tsx` work (`npx tsx --version`),
  then reopen the workspace.
- **Track B tools unavailable on macOS/Linux** — Expected; `crash-dump-mcp` needs Windows
  `cdb.exe`. Use Track A.
- **Dump won't load (mismatched symbols)** — Rebuild, then re-run `capture.ps1` /
  `capture.sh` to re-capture the dump.
- **Copilot blames the caller, not `allocate()`** — Redirect: "the crash is *inside*
  `allocator::allocate`".
- **`ctest` still passes after your fix** — You forgot to strip the `DISABLED_` prefix.
  Remove it and re-run.
- **Fix breaks the `noexcept` contract** — Re-read Constitution Article 1: no exceptions;
  return a status / `nullptr`.
- **Burning too many premium requests** — See Section 5.2: batch asks, use Plan mode,
  lighter models, fresh threads.

---

## 7. Try it yourself

### Exercise 1 — re-walk Phase 1 unaided

**Goal:** open `BUG-001.dmp` cold (no script) and find the failing frame in under 5 minutes.
See [exercises/01-pinpoint.md](exercises/01-pinpoint.md).

### Exercise 2 — fix BUG-001 yourself

**Goal:** without copying the walk-through diff, produce your own fix + regression test that
`ctest` accepts. See [exercises/02-fix-with-test.md](exercises/02-fix-with-test.md).

---

## 8. Going further

- Read [docs/crash-dump-triage.md](docs/crash-dump-triage.md) for the canonical 3-phase pattern.
- Read [crash-dump-sdlc-workshop.md](crash-dump-sdlc-workshop.md) for the full CORE framework
  and agent anatomy behind Track B.
- Apply the pattern to one of your real production dumps — bring findings to office hours.

## Feedback

[retro-survey.md](retro-survey.md). 60 seconds.
