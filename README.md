# Crash Dump SDLC Workshop

> **Duration:** 3–4 hours · **Level:** Advanced · **Audience:** C++ game-engine developers
> · **Plan tier:** GitHub Copilot Business/Enterprise

A self-contained, instructor-led workshop that teaches a **fully automated, multi-agent
crash-dump SDLC** built on GitHub Copilot custom agents, MCP servers, Tree-of-Thought
analysis, git worktrees, and Human-in-the-Loop gates. The repo bundles both the **training
material** and a runnable **synthetic C++20 game-engine workspace** with seeded bugs.

## Start here

| Audience   | Document                                                                                                                                                                 |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Everyone   | [crash-dump-sdlc-workshop.md](crash-dump-sdlc-workshop.md) — the full workshop guide                                                                                     |
| Instructor | [facilitator-script.md](facilitator-script.md) · [slides.outline.md](slides.outline.md)                                                                                  |
| Learner    | [learner-guide.md](learner-guide.md) · [pre-work-email.md](pre-work-email.md)                                                                                            |
| Hands-on   | [exercises/01-pinpoint.md](exercises/01-pinpoint.md) · [exercises/02-fix-with-test.md](exercises/02-fix-with-test.md)                                                    |
| Reference  | [docs/architect.md](docs/architect.md) · [docs/cplusplus-knowledge.md](docs/cplusplus-knowledge.md) · [docs/crash-dump-sdlc-runbook.md](docs/crash-dump-sdlc-runbook.md) |

## Prerequisites

- **VS Code** + **GitHub Copilot** (Business or Enterprise) with agents enabled.
- **Node.js 18+** (for the two MCP servers; `npx`/`tsx` used to launch them).
- **CMake 3.28+**, **Ninja**, and a recent **C++20** compiler (MSVC 19.38+ / GCC 13+ /
  Clang 17+).
- **vcpkg** in manifest mode (provides EASTL + GoogleTest). Full clone, not `--depth 1`.
- **Git** (the `worktree-mcp` server drives `git worktree`).
- _(Live `.dmp` demo only)_ **Windows Debugging Tools** (`cdb.exe`) — optional; the manual
  Plan/Edit-mode exercises run cross-platform without a dump.

## Repository layout

```text
.github/
  agents/         # 6 crash-* custom agents (analyzer, planner, engineer, qa, validator, orchestrator)
  instructions/   # 3 scoped instruction files (applyTo-activated)
  prompts/        # 6 reusable prompt templates
  copilot-instructions.md
.vscode/mcp.json  # registers crash-dump-mcp + worktree-mcp
tools/
  crash-dump-mcp/ # MCP server wrapping cdb.exe (TypeScript)
  worktree-mcp/   # MCP server wrapping git worktree + cmake + ctest (TypeScript)
specs/            # constitution.md (ground truth) + per-feature specs
src/ include/ tests/   # the engine_demo C++ library + GoogleTest suite
apps/             # playable sandbox + poc
fixtures/         # seeded bugs + BUG-001 crash-dump reproducer & capture scripts
docs/             # knowledge files + runbook + agent-output dir (crash-reports/)
exercises/        # hands-on lab steps
```

## MCP server setup

The two MCP servers are registered in [.vscode/mcp.json](.vscode/mcp.json) and start
automatically when VS Code opens the folder. To install dependencies / pre-build:

```bash
( cd tools/crash-dump-mcp && npm install && npm run build )
( cd tools/worktree-mcp  && npm install && npm run build )
```

On first use, `crash-dump-mcp` prompts for `cdbPath` (the path to `cdb.exe`). This is only
needed for the live minidump-parsing demo on Windows.

## Capturing the BUG-001 crash dump

No binary dump is committed. Generate it locally from the reproducer:

```bash
cd fixtures/crash_dumps/BUG-001
./capture.sh      # Linux/macOS
# or: ./capture.ps1   (Windows)
```

The dump lands as `BUG-001.dmp.local` (or `.core.local`) — these `.local` artifacts are
git-ignored. See [fixtures/crash_dumps/BUG-001/README.md](fixtures/crash_dumps/BUG-001/README.md).

---

## The engine_demo workspace

This is a synthetic C++20 game-engine library — the **demo target** for the workshop. It is
**not** production code: it is intentionally small and contains seeded bugs.

## Build

Prerequisites: CMake 3.28+, Ninja, vcpkg (manifest mode), a recent C++20 compiler
(MSVC 19.38+ / GCC 13+ / Clang 17+).

```bash
cmake --preset default-debug
cmake --build --preset default-debug
ctest --preset default-debug --output-on-failure
```

The build is `-fno-exceptions -fno-rtti` everywhere. EASTL is the only allowed container
library in committed code.

## Run the playable sandbox

The `apps/sandbox/ea-sandbox` binary is the visual entry point — a 2D verlet-rope-plus-
particles scene driven by `engine_demo::physics::constraint_solver`,
`engine_demo::sim::game_loop`, `engine_demo::sim::rng`, and `engine_demo::frame_budget`.
The HUD surfaces the seeded-bug anchors live (BUG-002 wall/sim drift,
BUG-004 trace-digest divergence on reseed, BUG-006 frame-time graph).

```bash
# Interactive (opens a window):
./build/apps/sandbox/ea-sandbox

# Headless determinism trace (used by CI):
./build/apps/sandbox/ea-sandbox --headless --seed 42 --frames 600 --out trace.csv
```

Controls: `Space` pause / resume · `S` single-step · `R` reseed · `+` / `-` speed ·
`F1` deliberate crash (Session 02 demo) · `Esc` quit.

To opt out of building the sandbox (e.g., on a runner without raylib system deps):

```bash
cmake --preset default-debug -DENGINE_DEMO_BUILD_SANDBOX=OFF
```

See [apps/sandbox/README.md](apps/sandbox/README.md) for the bug-visibility cheat sheet.

## Seeded bugs

The workspace ships with intentionally seeded defects used by the workshop exercises. See
`fixtures/seeded-bugs.md` for the catalog. Do not fix them — they are the demo material.
The Session 02 anchor files are `fixtures/crash_dumps/BUG-001/` and
`src/engine_demo/allocator.cpp`.

## Constitution

The non-negotiables for any C++ in this workspace are codified in
[`specs/constitution.md`](specs/constitution.md): no exceptions, no RTTI, EASTL-first,
allocator-aware, deterministic. Every crash agent treats it as ground truth.
