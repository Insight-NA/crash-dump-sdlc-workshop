---
description: "Rules for crash dump analysis — MCP tool usage patterns, tree-of-thought structure, evidence standards."
applyTo: "docs/crash-reports/**-analysis.md,.github/chatmodes/crash-analyzer.chatmode.md"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Crash Dump Analysis Instructions

## Analysis protocol

When analyzing a crash dump, follow this exact sequence:

1. **Parse** → `parse_minidump` with both `.dmp` and `.pdb` paths
2. **Identify** → `get_call_stack` on the faulting thread
3. **Analyze** → `analyze_crash` for automated `!analyze -v` equivalent
4. **Contextualize** → `resolve_symbols` on ambiguous frames
5. **Inspect** → `get_memory_region` around the faulting address (if heap corruption suspected)

## Tree-of-thought requirements

Every analysis MUST produce exactly **3 hypotheses**, structured as:

```markdown
### Hypothesis A — [Title] (confidence: high|medium|low)

**Root cause**: One-sentence technical explanation.
**Evidence**: Bullet list of supporting data from the dump.
**Counter-evidence**: What would disprove this hypothesis.
**Fix direction**: What change would resolve this (do not write code).
```

### Hypothesis selection rules

- Hypotheses must be **mutually exclusive** or address different root causes
- At least one must be "high" confidence (if none warrant high, state why)
- If all evidence points to a single cause, the other two should be:
  - A deeper underlying cause (why did the immediate cause happen?)
  - An environmental cause (memory pressure, thread race, etc.)

## Evidence standards

- Stack frames MUST include source file + line number when PDB resolves
- Memory dumps MUST include at least 64 bytes before and after the faulting address
- Register state MUST be captured for the faulting thread
- Module list MUST note which modules have matched PDBs vs. export-only symbols

## Exception code reference

Map these Windows exception codes to investigation strategies:

| Code | Name | Primary investigation |
|------|------|---------------------|
| `0xC0000005` | ACCESS_VIOLATION | Check null deref vs. dangling ptr vs. buffer overrun |
| `0xC0000374` | HEAP_CORRUPTION | Look for double-free, buffer overflow, use-after-free |
| `0xC00000FD` | STACK_OVERFLOW | Check recursion depth, alloca abuse, thread stack size |
| `0xC0000409` | STACK_BUFFER_OVERRUN | Security cookie failure — buffer overwrite on stack |
| `0x80000003` | BREAKPOINT | Intentional break — check assert macros, `__debugbreak` |

## Output format

The analysis report file (`docs/crash-reports/<BUG-ID>-analysis.md`) MUST use this template:

```markdown
# Crash Analysis: <BUG-ID>

## Exception Summary
- **Code**: <hex> (<name>)
- **Address**: <faulting IP>
- **Thread**: <thread ID>
- **Process**: <exe name>
- **Timestamp**: <dump timestamp>

## Annotated Call Stack
| # | Module | Function | Source | Line |
|---|--------|----------|--------|------|
| 0 | ... | ... | ... | ... |

## Tree of Thought

### Hypothesis A — ...
### Hypothesis B — ...
### Hypothesis C — ...

## Evidence Artifacts
### Memory around faulting address
### Register state
### Module symbol status

## Recommended Next Steps
1. ...
```
