# Crash Reports

This directory holds all artifacts produced by the agentic crash dump resolution pipeline.

Files are generated automatically by the crash-dump SDLC agents. Do NOT create or edit
files here manually — they are produced by the following agents:

| File pattern | Producer | Phase |
|-------------|----------|-------|
| `BUG-XXX-analysis.md` | crash-analyzer | Intake/Analysis |
| `BUG-XXX-plan.md` | crash-planner | Planning |
| `BUG-XXX-impl-{a,b,c}.md` | crash-engineer | Implementation |
| `BUG-XXX-qa-{a,b,c}.md` | crash-qa | QA |
| `BUG-XXX-resolution.md` | crash-validator | Validation |
| `BUG-XXX-final.md` | crash-orchestrator | Final |

See [agentic-architecture.md](../agentic-architecture.md) for the full system design.
