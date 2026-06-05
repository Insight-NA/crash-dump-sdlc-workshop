# Agentic Crash Dump SDLC — Architecture

This document describes the fully automated (with human-in-the-loop gates) crash dump
resolution system built for C++ game engine developers.

## System Overview

```mermaid
flowchart TB
    subgraph INTAKE["Phase 1: Intake"]
        DMP[".dmp + .pdb files"]
        MCP1["crash-dump-mcp"]
        AN["crash-analyzer"]
        DMP --> MCP1
        MCP1 --> AN
    end

    subgraph ANALYSIS["Phase 2: Tree-of-Thought Analysis"]
        AN --> H1["Hypothesis A\n(high confidence)"]
        AN --> H2["Hypothesis B\n(medium confidence)"]
        AN --> H3["Hypothesis C\n(low confidence)"]
    end

    HITL1["⏸️ HITL Gate #1\nHuman reviews analysis"]
    H1 & H2 & H3 --> HITL1

    subgraph PLANNING["Phase 3: Resolution Planning"]
        PL["crash-planner"]
        HITL1 --> PL
        PL --> WT1["Worktree A\nfix/BUG-XXX-a"]
        PL --> WT2["Worktree B\nfix/BUG-XXX-b"]
        PL --> WT3["Worktree C\nfix/BUG-XXX-c"]
    end

    HITL2["⏸️ HITL Gate #2\nHuman approves plan"]
    WT1 & WT2 & WT3 --> HITL2

    subgraph EXECUTION["Phase 4: Parallel Fix Execution"]
        HITL2 --> ENG["crash-engineer"]
        ENG --> FA["Fix A\n(in worktree A)"]
        ENG --> FB["Fix B\n(in worktree B)"]
        ENG --> FC["Fix C\n(in worktree C)"]
    end

    subgraph VALIDATION["Phase 5: QA + Validation"]
        FA & FB & FC --> QA["crash-qa"]
        QA --> BUILD["cmake_build\n(all 3 branches)"]
        BUILD --> TEST["run_tests\n(full suite × 3)"]
        TEST --> REG["Regression test\n(new, per bug)"]
        REG --> VAL["crash-validator"]
    end

    HITL3["⏸️ HITL Gate #3\nHuman selects branch to merge"]
    VAL --> HITL3

    subgraph MERGE["Phase 6: Merge + Cleanup"]
        HITL3 --> ORC["crash-orchestrator"]
        ORC --> MRG["git merge\n(selected branch)"]
        MRG --> CLN["Remove worktrees\nCleanup branches"]
        CLN --> RPT["Final report\n+ audit trail"]
    end
```

## Agent Roles

| Agent | Mode File | Role | Tools |
|-------|-----------|------|-------|
| Crash Analyzer | `crash-analyzer.chatmode.md` | Parse dumps, produce tree-of-thought diagnosis | crash-dump-mcp |
| Crash Planner | `crash-planner.chatmode.md` | Design 3 parallel fix strategies | worktree-mcp |
| Crash Engineer | `crash-engineer.chatmode.md` | Implement fixes in isolated worktrees | worktree-mcp, terminal |
| Crash QA | `crash-qa.chatmode.md` | Run tests, generate regression tests | worktree-mcp, terminal |
| Crash Validator | `crash-validator.chatmode.md` | Present comparison + recommendation | worktree-mcp (read) |
| Crash Orchestrator | `crash-orchestrator.chatmode.md` | Coordinate pipeline, enforce HITL gates | all |

## MCP Server Architecture

```mermaid
flowchart LR
    subgraph "crash-dump-mcp"
        T1["parse_minidump"]
        T2["get_call_stack"]
        T3["resolve_symbols"]
        T4["analyze_crash"]
        T5["get_thread_context"]
        T6["list_modules"]
        T7["get_memory_region"]
    end

    subgraph "worktree-mcp"
        W1["create_worktree"]
        W2["list_worktrees"]
        W3["remove_worktree"]
        W4["cmake_build"]
        W5["run_tests"]
        W6["worktree_diff"]
    end

    CDB["cdb.exe\n(WinDbg CLI)"] --> T1 & T2 & T3 & T4 & T5 & T6 & T7
    GIT["git"] --> W1 & W2 & W3 & W6
    CMAKE["cmake / ctest"] --> W4 & W5
```

## File-Based Handoff Protocol

Agents communicate exclusively through markdown files in `docs/crash-reports/`:

```mermaid
sequenceDiagram
    participant H as Human Engineer
    participant O as Orchestrator
    participant A as Analyzer
    participant P as Planner
    participant E as Engineer
    participant Q as QA Agent
    participant V as Validator

    H->>O: "I have a crash dump"
    O->>A: Invoke with .dmp path
    A->>A: Parse dump, tree-of-thought
    A-->>O: writes BUG-XXX-analysis.md
    O->>H: ⏸️ HITL Gate #1
    H->>O: "Approved"
    O->>P: Invoke with analysis
    P->>P: Create 3 worktrees + plan
    P-->>O: writes BUG-XXX-plan.md
    O->>H: ⏸️ HITL Gate #2
    H->>O: "Approved"
    O->>E: Invoke with plan (×3 branches)
    E->>E: Implement fixes, build, test
    E-->>O: writes BUG-XXX-impl-{a,b,c}.md
    O->>Q: Invoke with implementations
    Q->>Q: Full test suite + regression test
    Q-->>O: writes BUG-XXX-qa-{a,b,c}.md
    O->>V: Invoke with QA results
    V-->>O: writes BUG-XXX-resolution.md
    O->>H: ⏸️ HITL Gate #3
    H->>O: "Merge Branch A"
    O->>O: git merge, cleanup worktrees
    O-->>H: writes BUG-XXX-final.md
```

## HITL Gates

| Gate | After Phase | Decision Options | Blocks |
|------|-------------|-----------------|--------|
| #1 | Analysis | Approve / Reject / Revise | Planning |
| #2 | Planning | Approve / Reject / Revise | Implementation |
| #3 | Validation | Merge X / Merge Y / Reject All / Revise | Merge |

**Non-negotiable**: No gate may be bypassed, even if all automated checks pass.

## Directory Structure

```
crash-dump-sdlc-workshop/
├── .github/
│   ├── chatmodes/
│   │   ├── crash-analyzer.chatmode.md
│   │   ├── crash-planner.chatmode.md
│   │   ├── crash-engineer.chatmode.md
│   │   ├── crash-qa.chatmode.md
│   │   ├── crash-validator.chatmode.md
│   │   └── crash-orchestrator.chatmode.md
│   ├── instructions/
│   │   ├── crash-dump-analysis.instructions.md
│   │   ├── crash-fix-engineering.instructions.md
│   │   └── resolution-tracking.instructions.md
│   └── prompts/
│       ├── crash-dump-intake.prompt.md
│       ├── tree-of-thought-analysis.prompt.md
│       ├── resolution-brief.prompt.md
│       ├── regression-test-generation.prompt.md
│       ├── fix-validation.prompt.md
│       └── resolution-report.prompt.md
├── .vscode/
│   └── mcp.json
├── tools/
│   ├── crash-dump-mcp/          ← MCP server (cdb.exe wrapper)
│   └── worktree-mcp/            ← MCP server (git worktree + cmake)
├── docs/
│   └── crash-reports/           ← All artifacts land here
└── tests/
    └── regression/              ← Permanent regression tests
```

## Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| MCP Servers | TypeScript + `@modelcontextprotocol/sdk` | Tool abstraction layer |
| Debugger | cdb.exe (Windows Debugging Tools) | Crash dump parsing |
| Build | CMake 3.28+ / Ninja | C++ compilation |
| Test | GoogleTest (ctest) | Validation |
| VCS | Git worktrees | Parallel branch isolation |
| Language | C++20 (MSVC/GCC/Clang) | Target codebase |
| Containers | EASTL | EA Standard Template Library |
