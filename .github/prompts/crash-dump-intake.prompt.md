---
description: "Entry prompt for crash dump intake — loads the dump, sets up symbols, triggers initial analysis."
mode: "crash-analyzer"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Crash Dump Intake

You are beginning a crash dump analysis session. Follow this protocol exactly.

## Inputs required

- `$DUMP_PATH` — absolute path to the `.dmp` minidump file
- `$PDB_PATH` — path to directory containing `.pdb` symbol files
- `$BUG_ID` — bug tracking identifier (format: BUG-NNN)

## Protocol

### Step 1: Load the dump

```
Use parse_minidump with dumpPath=$DUMP_PATH and pdbPath=$PDB_PATH
```

Record the exception code, process name, thread count.

### Step 2: Verify symbols

```
Use list_modules to check PDB match status
```

If fewer than 50% of game modules have matched PDBs, STOP and report:
> ⚠️ Insufficient symbols — request matching PDBs from the build server.

### Step 3: Extract call stack

```
Use get_call_stack on the faulting thread (maxFrames: 50)
```

### Step 4: Run automated analysis

```
Use analyze_crash for the !analyze -v equivalent
```

### Step 5: Deep inspection (conditional)

If exception is `HEAP_CORRUPTION` or `ACCESS_VIOLATION`:
```
Use get_memory_region around the faulting address (256 bytes)
Use get_thread_context on the faulting thread
```

### Step 6: Correlate with source

Read the source files referenced in the top 5 call stack frames.
Search the codebase for patterns related to the crash (e.g., the faulting function name).

### Step 7: Generate tree-of-thought

Produce exactly 3 hypotheses following the format in:
`#file:.github/instructions/crash-dump-analysis.instructions.md`

### Step 8: Write analysis report

Write the complete analysis to: `docs/crash-reports/$BUG_ID-analysis.md`

### Step 9: HITL Gate

Present the analysis summary and hypotheses to the human. Wait for approval
before handing off to `crash-planner`.
