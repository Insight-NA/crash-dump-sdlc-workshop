# Crash Dump SDLC Runbook

> **For**: C++ game engine developers using GitHub Copilot with custom agents
> **Prerequisite**: Windows Debugging Tools installed, `.dmp` + `.pdb` available

This runbook walks you through the 10-step agentic crash dump resolution process
from initial dump file to merged fix.

---

## Prerequisites

Before starting, ensure:

1. **Windows Debugging Tools** installed (provides `cdb.exe`)
   - Install via Windows SDK or `winget install Microsoft.WinDbg`
2. **Node.js 20+** for MCP servers
3. **CMake 3.28+** and **Ninja** for builds
4. **Git** with worktree support
5. **VS Code** with GitHub Copilot extension
6. Matching `.pdb` symbol files for the crashing build

## Quick Start

```bash
# 1. Install MCP server dependencies
cd tools/crash-dump-mcp && npm install && cd ../..
cd tools/worktree-mcp && npm install && cd ../..

# 2. Open VS Code — MCP servers auto-start via .vscode/mcp.json

# 3. Switch to crash-orchestrator mode and say:
#    "I have a crash dump at C:\dumps\game.dmp with symbols at C:\symbols\"
```

---

## Step-by-Step Process

### Step 1: Intake — Load the Crash Dump

**Mode**: `crash-orchestrator` → delegates to `crash-analyzer`

**What happens**:
- The `crash-dump-mcp` server opens the `.dmp` file via `cdb.exe`
- Symbol files (`.pdb`) are loaded and verified
- Basic dump metadata is extracted (exception code, thread count, modules)

**You provide**:
- Path to the `.dmp` file
- Path to the `.pdb` symbol directory
- Bug ID (e.g., `BUG-001`)

**Output**: Dump loaded confirmation, symbol match report

---

### Step 2: Analysis — Tree-of-Thought Diagnosis

**Mode**: `crash-analyzer`

**What happens**:
- Full call stack extracted for the faulting thread
- Automated `!analyze -v` equivalent runs
- Source files are correlated with stack frames
- **3 root-cause hypotheses** are generated (tree-of-thought)

**Output**: `docs/crash-reports/BUG-XXX-analysis.md`

---

### Step 3: HITL Gate #1 — Review Analysis

**Mode**: `crash-orchestrator`

**Your action**: Review the 3 hypotheses. Ask questions. Then respond:
- ✅ **APPROVE** — hypotheses are reasonable, proceed to planning
- ❌ **REJECT** — analysis is wrong, provide correction
- 🔄 **REVISE** — adjust a specific hypothesis

---

### Step 4: Planning — Design Parallel Fix Strategies

**Mode**: `crash-planner`

**What happens**:
- Each hypothesis is converted into a fix strategy
- 3 worktree branches are defined in the plan (created later by the engineer)
- Acceptance criteria defined per branch
- Risk assessment produced

**Output**: `docs/crash-reports/BUG-XXX-plan.md` (worktrees not yet created)

---

### Step 5: HITL Gate #2 — Approve Plan

**Mode**: `crash-orchestrator`

**Your action**: Review the 3 fix strategies. Check that:
- Each addresses a different root cause
- File modifications are reasonable
- Risk assessment makes sense

Respond: APPROVE / REJECT / REVISE

---

### Step 6: Implementation — Fix in Parallel Worktrees

**Mode**: `crash-engineer`

**What happens** (per branch):
1. Engineer creates the assigned worktree from the approved plan (`create_worktree`)
2. Reads the plan for their assigned branch
3. Implements the minimum viable fix
4. Runs `cmake_build` — must compile cleanly
5. Runs `run_tests` — must maintain pass rate
6. Documents changes

**Output**: `docs/crash-reports/BUG-XXX-impl-{a,b,c}.md`

**C++ Rules enforced**:
- No exceptions, no RTTI, EASTL-only, allocator-aware
- `[[nodiscard]]` on factories, `noexcept` on moves

---

### Step 7: QA — Test All Branches

**Mode**: `crash-qa`

**What happens**:
1. Full test suite run on each worktree
2. Regression test generated (must fail on base, pass on fix)
3. Verdicts produced: PASS / FAIL / CONDITIONAL

**Output**: `docs/crash-reports/BUG-XXX-qa-{a,b,c}.md`

---

### Step 8: Validation — Branch Comparison

**Mode**: `crash-validator`

**What happens**:
- All artifacts synthesized into comparison table
- Branches ranked by: correctness, test health, minimal change, compliance
- Recommendation produced with justification

**Output**: `docs/crash-reports/BUG-XXX-resolution.md`

---

### Step 9: HITL Gate #3 — Select Branch to Merge

**Mode**: `crash-orchestrator`

**Your action**: Review the resolution brief. Decide:
- ✅ **MERGE Branch X** — apply the recommended fix
- ✅ **MERGE Branch Y** — override with different branch
- ❌ **REJECT ALL** — no fix is acceptable
- 🔄 **REVISE** — request changes to a specific branch

---

### Step 10: Merge + Cleanup

**Mode**: `crash-orchestrator`

**What happens**:
1. Selected branch merged to `main`
2. All worktrees removed
3. Orphaned branches deleted
4. Final report generated with full audit trail
5. Regression test confirmed in CI

**Output**: `docs/crash-reports/BUG-XXX-final.md`

---

## Troubleshooting

### "Insufficient symbols"

The analyzer reports <50% PDB match. Solutions:
1. Ensure `.pdb` files match the EXACT build that produced the `.dmp`
2. Check symbol server configuration in `CDB_PATH` environment
3. Try: `set _NT_SYMBOL_PATH=srv*C:\symbols*https://msdl.microsoft.com/download/symbols`

### "Build fails in worktree"

A fix doesn't compile. The engineer will:
1. Check compiler output for the specific error
2. Fix compilation issues (often missing includes after code changes)
3. Re-run build — must hit zero errors before proceeding

### "All branches fail QA"

No fix passes the full test suite. Options:
1. Check if failures are pre-existing (not caused by the fix)
2. REVISE a branch with targeted changes
3. REJECT ALL and request manual investigation

### "Regression test passes on both"

The new test doesn't distinguish fixed from unfixed code. The QA agent will:
1. Make the test more specific to the crash conditions
2. Test with sanitizers enabled (ASan, UBSan)
3. Add edge-case inputs that trigger the specific code path

---

## Configuration

### Environment variables

| Variable | Purpose | Default |
|----------|---------|---------|
| `CDB_PATH` | Path to `cdb.exe` | `cdb.exe` (on PATH) |
| `DEBUG_CDB` | Enable verbose CDB output | `""` (disabled) |

### CMake presets

The project uses `CMakePresets.json`. The default dev preset is `dev-msvc`.
Worktree builds use the same preset unless specified otherwise.

### MCP server registration

Both servers are registered in `.vscode/mcp.json` and auto-start when
VS Code opens the workspace. No manual server management needed.
