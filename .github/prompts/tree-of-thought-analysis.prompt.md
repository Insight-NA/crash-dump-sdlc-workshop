---
description: "Prompt for generating tree-of-thought analysis from crash data — produces 3 ranked hypotheses."
mode: "crash-analyzer"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Tree-of-Thought Crash Analysis

Given the crash dump data you have collected, generate a rigorous tree-of-thought
analysis with exactly 3 distinct root-cause hypotheses.

## Thinking framework

For each hypothesis, reason through:

1. **What happened**: the immediate technical cause
2. **Why it happened**: the underlying code defect
3. **When it happens**: reproduction conditions
4. **Where in code**: specific file(s) and line(s)
5. **Confidence level**: based on evidence strength

## Confidence calibration

- **High**: Stack trace directly shows the defect, source code confirms, reproducible
- **Medium**: Stack trace is suggestive, source code has the pattern, but could be coincidence
- **Low**: Circumstantial — thread timing, memory layout dependent, intermittent

## Diversity requirements

The 3 hypotheses MUST explore different failure modes:

- **Hypothesis A**: The most obvious/direct cause (often high confidence)
- **Hypothesis B**: A deeper systemic cause that could manifest as this crash
- **Hypothesis C**: An environmental/timing cause (race condition, memory pressure, etc.)

## Anti-patterns (do NOT produce these)

- ❌ Three hypotheses that are the same bug described differently
- ❌ "Maybe the pointer is null" without identifying WHERE it became null
- ❌ Hypotheses without specific file/line references
- ❌ Hypotheses that blame "undefined behavior" without identifying the UB source

## Output

Use the exact format from `crash-dump-analysis.instructions.md` Hypothesis sections.
Each hypothesis must reference specific evidence from the dump analysis.
