---
description: "Rules for resolution tracking — file naming, artifact lifecycle, HITL gate format, audit trail."
applyTo: "docs/crash-reports/**"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Resolution Tracking Instructions

## File naming convention

All crash resolution artifacts live under `docs/crash-reports/` with strict naming:

```
docs/crash-reports/
├── <BUG-ID>-analysis.md       ← crash-analyzer output
├── <BUG-ID>-plan.md           ← crash-planner output
├── <BUG-ID>-impl-a.md         ← crash-engineer output (branch A)
├── <BUG-ID>-impl-b.md         ← crash-engineer output (branch B)
├── <BUG-ID>-impl-c.md         ← crash-engineer output (branch C)
├── <BUG-ID>-qa-a.md           ← crash-qa output (branch A)
├── <BUG-ID>-qa-b.md           ← crash-qa output (branch B)
├── <BUG-ID>-qa-c.md           ← crash-qa output (branch C)
├── <BUG-ID>-resolution.md     ← crash-validator output (HITL gate)
└── <BUG-ID>-final.md          ← crash-orchestrator final report
```

## BUG-ID format

`BUG-NNN` where NNN is a zero-padded 3-digit number. Examples: `BUG-001`, `BUG-042`.

## HITL gate format

Every HITL gate MUST use this exact markdown structure so it is visually distinct, and MUST be advanced by clicking a handoff button — never by asking the human to type a command:

```markdown
---

### ⏸️ HUMAN IN THE LOOP — Decision Required

**Context**: <one sentence about what was just completed>

**Objective**: <the single decision the human is being asked to make>

**Requirements**: <what the human should review above before deciding>

**Expectations**: Use the handoff buttons below — no typing required.

▶ ✅ **Approve / proceed** — advances to the next agent
▶ 🔄 **Revise** — sends feedback back for changes
▶ ❌ **Reject** — stops or closes this path

**The buttons ARE the gate; clicking one pre-fills the required token. Awaiting the human's selection before proceeding.**

---
```

The exact button labels are defined in each agent's `handoffs` frontmatter. Never write "type APPROVE", "say create worktrees", or "say implement" — always point the human at the button.

## Artifact lifecycle

| Phase | Created by | Consumed by | Immutable after |
|-------|-----------|-------------|-----------------|
| analysis | crash-analyzer | crash-planner | plan creation |
| plan | crash-planner | crash-engineer | HITL gate #2 approval |
| impl-{a,b,c} | crash-engineer | crash-qa | QA start |
| qa-{a,b,c} | crash-qa | crash-validator | validator start |
| resolution | crash-validator | human + orchestrator | human decision |
| final | crash-orchestrator | archives | never modified |

Once an artifact passes its "immutable after" point, it MUST NOT be modified.
If corrections are needed, append an `## Addendum` section — do not rewrite history.

## Audit trail requirements

The final report (`<BUG-ID>-final.md`) MUST contain:

1. **Timeline**: timestamped log of each phase start/end
2. **Decisions**: each HITL gate response recorded verbatim
3. **Branch selected**: which of A/B/C was merged and why
4. **Branches discarded**: why the other two were not selected
5. **Regression test**: name and location of the permanent regression test
6. **Commit SHA**: the merge commit that resolved the bug

## Status tracking

Each artifact's front-matter should include:

```yaml
---
bug_id: BUG-NNN
phase: analysis | plan | implement | qa | validate | final
status: in-progress | complete | blocked | rejected
created: YYYY-MM-DD HH:MM
author: <agent-mode-name>
---
```
