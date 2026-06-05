---
description: "Rules for crash fix engineering — code standards, worktree workflow, build/test protocol, constitution compliance."
applyTo: "docs/crash-reports/**-impl-*.md,.github/chatmodes/crash-engineer.chatmode.md"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# Crash Fix Engineering Instructions

## Worktree workflow

Every fix attempt happens in an isolated git worktree. NEVER modify the main working tree.

```
main repo/
├── .worktrees/
│   ├── fix-BUG-XXX-a/    ← hypothesis A fix
│   ├── fix-BUG-XXX-b/    ← hypothesis B fix
│   └── fix-BUG-XXX-c/    ← hypothesis C fix
└── (main working tree — read-only during fix phase)
```

### Protocol per branch

1. Switch to your assigned worktree path
2. Read the resolution plan for your branch's strategy
3. Implement the minimum viable fix (smallest change that resolves the crash)
4. Build with `cmake_build` — must produce ZERO errors
5. Run tests with `run_tests` — must maintain or improve pass rate
6. Document your changes in the implementation notes

## C++ coding rules (Constitution compliance)

All fixes MUST comply with the project constitution. Key rules:

### No exceptions
```cpp
// WRONG
throw std::runtime_error("allocation failed");

// RIGHT — return error via [[nodiscard]] result type
[[nodiscard]] eastl::expected<T, ErrorCode> allocate(size_t n);
```

### No RTTI
```cpp
// WRONG
if (auto* derived = dynamic_cast<DerivedSystem*>(system)) { ... }

// RIGHT — use type tags, component IDs, or virtual dispatch
if (system->type_id() == DerivedSystem::kTypeId) { ... }
```

### EASTL-first
```cpp
// WRONG
std::vector<Entity> entities;
std::unordered_map<uint32_t, Component*> lookup;

// RIGHT
eastl::vector<Entity, FrameAllocator> entities;
eastl::hash_map<uint32_t, Component*, FrameAllocator> lookup;
```

### Allocator-aware
```cpp
// WRONG — implicit allocator
eastl::vector<int> temp;

// RIGHT — explicit allocator from context
eastl::vector<int, FrameAllocator> temp(frame_alloc);
```

### Move semantics
```cpp
// All move ctors and move-assignment MUST be noexcept
ComponentPool(ComponentPool&& other) noexcept;
ComponentPool& operator=(ComponentPool&& other) noexcept;
```

### [[nodiscard]] on factories and error-returning functions
```cpp
[[nodiscard]] Entity create_entity();
[[nodiscard]] bool try_reserve(size_t capacity);
```

## Minimum viable fix principle

- Fix the crash, nothing else
- Do not refactor surrounding code
- Do not add features
- Do not change APIs unless the bug IS the API
- One logical change per branch

## Build validation

After every code change:
1. `cmake_build` with the project's dev preset
2. If build fails → fix compilation errors (do NOT skip)
3. `run_tests` with no filter (full suite)
4. If tests fail → determine if failure is pre-existing or introduced
5. Document results in implementation notes

## Implementation notes template

```markdown
# Implementation: <BUG-ID> Branch <A|B|C>

## Strategy
<From resolution plan>

## Changes
| File | Change | Rationale |
|------|--------|-----------|
| ... | ... | ... |

## Build Result
- Preset: `dev-msvc`
- Status: PASS / FAIL
- Warnings: <count> (N new)
- Errors: 0

## Test Result
- Total: N
- Passed: N
- Failed: N
- New failures: <list or "none">

## Constitution Checklist
- [ ] No exceptions
- [ ] No RTTI
- [ ] EASTL-only containers
- [ ] Allocator-aware
- [ ] [[nodiscard]] on appropriate functions
- [ ] noexcept on move operations
```
