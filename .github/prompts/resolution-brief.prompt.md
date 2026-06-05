---
description: "Prompt for crash-planner to produce a resolution brief with 3 parallel fix branches."
mode: "crash-planner"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Resolution Brief Generation

You are converting a crash analysis (with 3 hypotheses) into an actionable resolution
plan with 3 parallel fix branches.

## Inputs

- Analysis report: `docs/crash-reports/$BUG_ID-analysis.md`
- Project source: the codebase under `src/` and `include/`
- Constitution: `specs/constitution.md`

## Planning protocol

### Step 1: Read and validate the analysis

Confirm the analysis has:
- [ ] 3 distinct hypotheses
- [ ] Source file references for each
- [ ] At least one high-confidence hypothesis

### Step 2: Design fix strategies

For each hypothesis, design a minimal fix strategy:

| Branch | Hypothesis | Strategy | Files to modify | Risk |
|--------|-----------|----------|-----------------|------|
| A | ... | ... | ... | low/medium/high |
| B | ... | ... | ... | low/medium/high |
| C | ... | ... | ... | low/medium/high |

**Strategy types**:
- `bounds-check` — add missing validation before the operation
- `lifetime-fix` — correct ownership/lifetime of a resource
- `ordering-fix` — reorder operations to prevent race/corruption
- `type-fix` — correct type widths, casts, or conversions
- `init-fix` — ensure proper initialization before use
- `logic-fix` — correct a logical error in control flow

### Step 3: Define acceptance criteria

Per branch, define pass/fail criteria:
- Which existing test(s) should now pass that previously crashed?
- What NEW test should be added to prevent regression?
- What build constraints apply? (zero warnings, specific sanitizers, etc.)

### Step 4: Create worktrees

```
Use create_worktree for each branch:
  - fix/$BUG_ID-a (from main)
  - fix/$BUG_ID-b (from main)
  - fix/$BUG_ID-c (from main)
```

### Step 5: Write the plan

Output to: `docs/crash-reports/$BUG_ID-plan.md`

### Step 6: HITL Gate

Present the plan summary to the human with the gate marker from
`resolution-tracking.instructions.md`. Do NOT proceed until approved.
