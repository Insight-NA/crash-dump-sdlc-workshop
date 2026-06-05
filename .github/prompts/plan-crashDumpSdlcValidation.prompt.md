# Plan: Validate AI Agent Crash Dump SDLC Demo (10 Steps)

## TL;DR
Collaborative walkthrough of Section 1 of the workshop — the 10-step BUG-001 demo pipeline.
Validate each step works end-to-end, document gaps, and produce an improvement report.

---

## User Environment
- Platform: macOS + Windows available (full demo possible)
- Node.js: installed
- Wants: Pre-flight first, then clean demo run
- Allocator: reset to buggy state needed

---

## Pre-flight Findings (discovered via workspace exploration)

### Critical Blockers
| Issue | Impact | Fix Needed |
|-------|--------|-----------|
| `.vscode/mcp.json` MISSING | MCP servers not wired to VS Code | Create the file |
| `tools/crash-dump-mcp/` NOT BUILT | No `dist/` directory — `npm run build` not run | Run `npm install && npm run build` |
| `tools/worktree-mcp/` NOT BUILT | Same — no `dist/` | Run `npm install && npm run build` |
| `fixtures/crash_dumps/BUG-001/*.dmp` MISSING | No crash dump to feed Step 1 | Run `capture.sh` / `capture.ps1` |
| `src/engine_demo/allocator.cpp` is POST-FIX state | Demo expects BUGGY code as starting point | Reset to buggy version |
| macOS platform | `crash-dump-mcp` uses `cdb.exe` (Windows only) | Only worktree-mcp tools run natively on macOS |

### State Issues
| File | Current State | Expected State |
|------|--------------|----------------|
| `src/engine_demo/allocator.cpp` | FIXED (bounds check BEFORE m_offset mutation) | BUGGY (bounds check AFTER m_offset mutation) |
| `tests/engine_demo/test_allocator.cpp` | Test is active — no `DISABLED_` prefix | Should be `DISABLED_` at demo start |
| `docs/crash-reports/` | Empty (no artifacts) | Expected — starts empty |

### What Exists ✅
- All 6 agent files in `.github/agents/`
- All 6 prompt templates in `.github/prompts/`
- All 4 instruction files in `.github/instructions/`
- `specs/constitution.md` (8 articles)
- Source for both MCP servers (unbuilt)
- `fixtures/crash_dumps/BUG-001/repro.cpp` and capture scripts

---

## Phase 0 — Pre-flight Setup

### Step 0a — Build the MCP servers (macOS)
```bash
cd tools/crash-dump-mcp && npm install && npm run build
cd ../worktree-mcp && npm install && npm run build
```
✅ Validate: both produce a `dist/index.js`.

### Step 0b — Create `.vscode/mcp.json`
File missing entirely. Must be created with:
- `crash-dump-mcp` entry: `type: stdio`, command: `node dist/index.js`, needs `${input:cdbPath}` for Windows cdb.exe path
- `worktree-mcp` entry: `type: stdio`, command: `node dist/index.js`

### Step 0c — Reset `allocator.cpp` to the buggy state
The seeded bug (BUG-001): `m_offset` mutated by `align_up` **before** bounds check.
Buggy pattern:
```cpp
m_offset = align_up(m_offset, alignment);   // ← advances FIRST
if (m_offset + n > m_capacity) return nullptr;  // ← bounds check AFTER (too late)
```

### Step 0d — Add `DISABLED_` prefix to regression test
In `tests/engine_demo/test_allocator.cpp` line 31:
```
BEFORE: TEST(allocator, third_aligned_alloc_does_not_overrun_arena)
AFTER:  TEST(allocator, DISABLED_third_aligned_alloc_does_not_overrun_arena)
```
The crash-qa agent's job is to re-enable this as part of fix validation.

### Step 0e — Generate the crash dump (Windows)
```powershell
cd fixtures\crash_dumps\BUG-001
.\capture.ps1
```
✅ Validate: `BUG-001.dmp` and matching `.pdb` appear in the directory.

### Step 0f — Confirm VS Code sees MCP servers
Open VS Code MCP panel (or reload window) → verify both `crash-dump-mcp` and `worktree-mcp`
appear as connected/available.

---

## Phase 1 — Demo Steps 1–10

### Step 1 — Intake: Load the Crash Dump
**Agent:** `crash-orchestrator`
**Prompt to paste in Copilot Chat:**
```
I have a crash dump at fixtures/crash_dumps/BUG-001/BUG-001.dmp with symbols at
fixtures/crash_dumps/BUG-001/. Bug ID is BUG-001.
```
✅ Validate: crash-dump-mcp connects; exception code `0xC0000005` (ACCESS_VIOLATION) returned in metadata.

### Step 2 — Analysis: Tree-of-Thought Diagnosis
**Agent:** `crash-analyzer` (delegated by orchestrator)
✅ Validate: `docs/crash-reports/BUG-001-analysis.md` created with exactly 3 hypotheses,
each with file+line citations and confidence levels (at least one "high").

**Expected 3 hypotheses:**
| Hypothesis | Confidence | Root Cause |
|------------|------------|-----------|
| A | High | Bounds check ordering: `m_offset` advances before validation |
| B | Medium | Alignment calc: `align_up()` produces incorrect offset for edge cases |
| C | Low | Arena capacity: buffer too small for 3 aligned allocations |

### Step 3 — HITL Gate #1: Human Reviews Analysis
**Expected gate format:**
```
### ⏸️ HUMAN IN THE LOOP — Decision Required
Context: Tree-of-thought analysis complete. 3 hypotheses generated.
Options:
1. ✅ APPROVE — proceed to planning with all 3 hypotheses
2. ❌ REJECT — analysis is fundamentally wrong
3. 🔄 REVISE — adjust specific hypotheses
Awaiting response.
```
**Response:** Type `APPROVE`
✅ Validate: Gate renders with all 3 options; APPROVE proceeds to Step 4.

### Step 4 — Planning: Design 3 Fix Strategies
**Agent:** `crash-planner` (delegated by orchestrator)
✅ Validate:
- `docs/crash-reports/BUG-001-plan.md` created with 3 strategies
- 3 git worktrees created at `.worktrees/fix-BUG-001-{a,b,c}/`
- Each branch has defined acceptance criteria

**Expected branches:**
| Branch | Strategy | Approach |
|--------|----------|----------|
| `fix/BUG-001-a` | `bounds-check` | Move bounds check before `m_offset` mutation |
| `fix/BUG-001-b` | `ordering-fix` | Compute aligned offset + end in locals, validate, then commit |
| `fix/BUG-001-c` | `type-fix` | Change capacity to account for max alignment overhead |

### Step 5 — HITL Gate #2: Human Approves Plan
**Response:** Type `APPROVE`
✅ Validate: Gate records APPROVE decision; orchestrator delegates to crash-engineer.

### Step 6 — Implementation: Fix in Parallel Worktrees
**Agent:** `crash-engineer` (delegated by orchestrator, runs per branch)
✅ Validate per branch:
- `cmake_build` returns SUCCESS, 0 errors in each worktree
- `docs/crash-reports/BUG-001-impl-{a,b,c}.md` created for each branch
- Fixed code in Branch B uses local variables (canonical safe pattern):
```cpp
const auto aligned = align_up(m_offset, alignment);
if (aligned + n > m_capacity) return nullptr;
m_offset = aligned + n;
```

### Step 7 — QA: Test All Branches
**Agent:** `crash-qa` (delegated by orchestrator)
✅ Validate:
- `docs/crash-reports/BUG-001-qa-{a,b,c}.md` created
- `DISABLED_third_aligned_alloc_does_not_overrun_arena` re-enabled by agent
- Test FAILS on unfixed base branch → PASSES on all 3 fixed branches
- All 24 tests pass on each fixed branch

### Step 8 — Validation: Branch Comparison
**Agent:** `crash-validator` (delegated by orchestrator)
✅ Validate:
- `docs/crash-reports/BUG-001-resolution.md` created
- Comparison table with scores present:

| Criteria (weight) | Branch A | Branch B | Branch C |
|------------------|----------|----------|----------|
| Correctness (40%) | ✅ Passes | ✅ Passes | ⚠️ Masks bug |
| Test health (30%) | 100% | 100% | 100% |
| Minimal change (20%) | 3 lines | 5 lines | 1 line |
| Constitution (10%) | ✅ | ✅ | ✅ |
| **Score** | **92** | **95** | **68** |

- Recommendation: Branch B

### Step 9 — HITL Gate #3: Human Selects Branch
**Response:** Type `MERGE Branch B`
✅ Validate: Gate records selection; orchestrator proceeds to merge.

### Step 10 — Merge + Cleanup
**Agent:** `crash-orchestrator`
✅ Validate:
- `fix/BUG-001-b` merged into `main`
- All 3 worktrees removed
- `docs/crash-reports/BUG-001-final.md` created with full audit trail
- Audit trail includes: timeline, all 3 HITL decisions, branch selected, commit SHA

---

## Findings Log (updated as we validate)

| Step | Status | Finding |
|------|--------|---------|
| 0a | 🔲 pending | Build crash-dump-mcp |
| 0a | 🔲 pending | Build worktree-mcp |
| 0b | 🔲 pending | Create .vscode/mcp.json |
| 0c | 🔲 pending | Reset allocator.cpp to buggy state |
| 0d | 🔲 pending | Add DISABLED_ prefix to regression test |
| 0e | 🔲 pending | Generate BUG-001.dmp on Windows |
| 0f | 🔲 pending | Confirm VS Code sees MCP servers |
| 1  | 🔲 pending | Intake — crash dump parsed |
| 2  | 🔲 pending | Analysis — BUG-001-analysis.md with 3 hypotheses |
| 3  | 🔲 pending | HITL Gate #1 — format correct, APPROVE works |
| 4  | 🔲 pending | Planning — plan.md + 3 worktrees |
| 5  | 🔲 pending | HITL Gate #2 — APPROVE works |
| 6  | 🔲 pending | Implementation — 3 branches build clean |
| 7  | 🔲 pending | QA — DISABLED_ test re-enabled, fails on base, passes on fix |
| 8  | 🔲 pending | Validation — comparison table, Branch B recommended |
| 9  | 🔲 pending | HITL Gate #3 — MERGE Branch B recorded |
| 10 | 🔲 pending | Merge — final.md with audit trail, worktrees cleaned |

---

## Improvement Report

| ID | Step | Issue | Recommendation |
|----|------|-------|----------------|
| I-01 | 0b | `.vscode/mcp.json` not in repo | Add to repo with `${input:cdbPath}` for portability |
| I-02 | 0c | `allocator.cpp` committed in post-fix state | Keep seeded bugs in the "buggy" state in repo; demo must not require manual reset |
| I-03 | 0d | Regression test not using `DISABLED_` prefix | Add `DISABLED_` prefix to `third_aligned_alloc_does_not_overrun_arena` before committing |
| I-04 | 0e | No pre-captured `.dmp` in repo | Consider committing a pre-captured `BUG-001.dmp` (binary, gitignored currently) or adding a headless Linux capture path |
| I-05 | 1–2 | `crash-dump-mcp` Windows-only (cdb.exe) | Document macOS fallback path; consider LLDB-based alternative for non-Windows facilitators |
