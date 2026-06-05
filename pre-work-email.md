# Session 02 — Pre-work email (T-2 days)

**Subject:** EA × Insight Copilot Workshop — Session 02 prep (~10 min)

---

Session 02 (Bug & Defect Fixing: Crash Dumps) runs **<DATE> at <TIME>**.

## Required (~5 min)

- Build the demo workspace at `default-debug`:

  ```bash
  # from the repo root
  cmake --preset default-debug
  cmake --build --preset default-debug
  ```

- Capture the BUG-001 fixture:

  ```bash
  ./fixtures/crash_dumps/BUG-001/capture.sh    # Linux
  # or
  .\fixtures\crash_dumps\BUG-001\capture.ps1   # Windows
  ```

  This produces `BUG-001.dmp.local` (or `.core.local`). Confirm the file exists.

## Recommended (~5 min)

- Re-skim the 3-phase triage pattern at [`docs/crash-dump-triage.md`](docs/crash-dump-triage.md).

## Bring

A real crash you remember struggling to triage. We'll discuss patterns in Q&A.

— Insight delivery team
