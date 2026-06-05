---
description: "Prompt for crash-qa to generate targeted regression tests for a specific crash bug."
mode: "crash-qa"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Regression Test Generation

Generate a regression test that specifically targets the crash scenario described in
the bug analysis. The test must FAIL on the unfixed code and PASS on the fixed code.

## Inputs

- Bug analysis: `docs/crash-reports/$BUG_ID-analysis.md`
- Fix implementation: `docs/crash-reports/$BUG_ID-impl-$BRANCH.md`
- Existing tests: `tests/` directory

## Test requirements

### Framework
- Use GoogleTest (GTest) — `TEST()` or `TEST_F()` macros
- File location: `tests/regression/test_$BUG_ID.cpp`
- Test name format: `RegressionBugNNN_<DescriptiveScenario>`

### C++ constraints (same as production code)
- C++20
- EASTL containers with explicit allocators
- No exceptions (`EXPECT_*` macros only, no `ASSERT_THROW`)
- No RTTI

### Test structure

```cpp
#include <gtest/gtest.h>
#include <EASTL/vector.h>
// ... relevant headers

// Reproduce the EXACT conditions that caused the crash.
// The test should trigger the same code path that crashed.
TEST(RegressionBugNNN, DescriptiveScenario)
{
    // ARRANGE: Set up the state that leads to the crash
    // ...

    // ACT: Perform the operation that crashed
    // ...

    // ASSERT: Verify correct behavior (no crash, correct output)
    EXPECT_EQ(result, expected);
}
```

### Verification protocol

1. Build the test in the UNFIXED base branch → it should crash or fail
2. Build the test in the FIXED worktree → it should pass
3. If the test passes on BOTH → it doesn't catch the bug, rewrite it

### Anti-patterns

- ❌ Tests that always pass regardless of the fix
- ❌ Tests that check unrelated behavior
- ❌ Tests with `sleep()` or timing-dependent assertions
- ❌ Tests that allocate unbounded memory

## Output

Write the test file and document in the QA verdict which branches pass/fail the regression test.
