---
description: "Prompt for crash-orchestrator to generate the final resolution report after merge."
mode: "crash-orchestrator"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Resolution Report Generation

After the human has approved a merge, generate the final resolution report that
serves as the permanent audit trail for this crash fix.

## Inputs

- All artifacts in `docs/crash-reports/$BUG_ID-*`
- The merge commit SHA
- The human's decision at each HITL gate

## Output

Write to `docs/crash-reports/$BUG_ID-final.md`:

```markdown
---
bug_id: $BUG_ID
phase: final
status: complete
created: <timestamp>
author: crash-orchestrator
---

# Final Resolution Report: $BUG_ID

## Timeline

| Time | Phase | Agent | Action |
|------|-------|-------|--------|
| ... | Intake | crash-analyzer | Dump parsed, 3 hypotheses generated |
| ... | HITL #1 | human | Approved analysis |
| ... | Planning | crash-planner | 3 worktrees created |
| ... | HITL #2 | human | Approved plan |
| ... | Implementation | crash-engineer | Fixes coded in 3 branches |
| ... | QA | crash-qa | Tests run, regression test added |
| ... | Validation | crash-validator | Branch X recommended |
| ... | HITL #3 | human | Approved merge of Branch X |
| ... | Merge | crash-orchestrator | Branch X merged to main |
| ... | Cleanup | crash-orchestrator | Worktrees removed |

## Root Cause

<2-3 sentence technical explanation of what caused the crash>

## Fix Applied

- **Branch merged**: fix/$BUG_ID-<x>
- **Commit SHA**: <sha>
- **Files changed**: <list>
- **Lines added/removed**: +N / -M

## Hypotheses Outcome

| Hypothesis | Confidence | Correct? | Notes |
|-----------|-----------|----------|-------|
| A | high | ✅/❌ | ... |
| B | medium | ✅/❌ | ... |
| C | low | ✅/❌ | ... |

## Regression Test

- **File**: `tests/regression/test_$BUG_ID.cpp`
- **Test name**: `RegressionBugNNN_Scenario`
- **Permanently in CI**: ✅

## Lessons Learned

<What could prevent similar crashes in the future? Coding pattern, review checklist item, etc.>

## Cleanup Confirmation

- [ ] Worktree fix/$BUG_ID-a removed
- [ ] Worktree fix/$BUG_ID-b removed
- [ ] Worktree fix/$BUG_ID-c removed
- [ ] No orphaned branches remain
- [ ] All intermediate artifacts preserved (not deleted)
```

## Post-merge actions

1. Remove all worktrees: `remove_worktree` for each
2. Verify no orphaned branches: `git branch --list "fix/$BUG_ID-*"`
3. Write the final report
4. Declare the pipeline complete
