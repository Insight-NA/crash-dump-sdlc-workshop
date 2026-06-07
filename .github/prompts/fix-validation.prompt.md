---
description: "Prompt for crash-validator to synthesize results and present final HITL gate."
mode: "crash-validator"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Fix Validation & HITL Gate

You are the final quality gate. Synthesize all artifacts into a clear recommendation
for the human engineer.

## Inputs

Gather and read ALL of these:
- `docs/crash-reports/$BUG_ID-analysis.md` — original diagnosis
- `docs/crash-reports/$BUG_ID-plan.md` — resolution strategy
- `docs/crash-reports/$BUG_ID-impl-{a,b,c}.md` — implementation notes
- `docs/crash-reports/$BUG_ID-qa-{a,b,c}.md` — QA verdicts

## Evaluation criteria

Rank branches by:

1. **Correctness**: Does the fix address the actual root cause? (weight: 40%)
2. **Test health**: Full suite pass rate + regression test passes (weight: 30%)
3. **Minimal change**: Fewest lines changed, lowest risk of side effects (weight: 20%)
4. **Constitution compliance**: Adherence to C++ rules (weight: 10%)

## Output: Resolution brief

Write to `docs/crash-reports/$BUG_ID-resolution.md`:

```markdown
# Resolution Brief: $BUG_ID

## Executive Summary
<3 sentences: what crashed, why, and which fix resolves it>

## Branch Comparison

| Criterion | Branch A | Branch B | Branch C |
|-----------|----------|----------|----------|
| QA Verdict | PASS/FAIL | ... | ... |
| Lines Changed | N | ... | ... |
| New Warnings | N | ... | ... |
| Regression Test | ✅/❌ | ... | ... |
| Addresses Root Cause | yes/partial/no | ... | ... |
| Constitution Compliant | ✅/❌ | ... | ... |

## Recommendation

**Merge Branch X** because:
1. <reason>
2. <reason>
3. <reason>

## Diff Summary (Branch X)
<files changed list — not full diff>

## Regression Test Confirmation
- Test name: `RegressionBugNNN_Scenario`
- Passes on fix: ✅
- Fails on base: ✅ (confirms it catches the bug)

---

### ⏸️ HUMAN IN THE LOOP — Decision Required

**Context**: Analysis complete, fixes implemented and tested, recommendation ready.

**Objective**: Choose how to resolve $BUG_ID.

**Requirements**: Review the branch comparison and recommendation above.

**Expectations**: Use the handoff buttons below — no typing required (edit `<branch>` on the merge button to override the recommendation):
1. ✅ **Merge Approved Branch** — apply the selected fix to main
2. 🔄 **Reject All — Request Revisions** — send feedback back to crash-engineer
3. ❌ **Reject All & Close** — abandon all branches and close the report

**The three buttons ARE the gate — clicking one pre-fills the required token. Awaiting the human's selection before proceeding.**

---
```

## Critical rules

- NEVER recommend a branch with QA verdict "FAIL"
- If ALL branches fail, recommend "REJECT ALL" and explain why
- Include the full HITL gate marker — do not abbreviate
- Keep the brief under 200 lines total
