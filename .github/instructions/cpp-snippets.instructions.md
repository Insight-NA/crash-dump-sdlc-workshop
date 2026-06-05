---
description: C++20 snippet authoring rules — EA game-engine conventions (no exceptions, no RTTI, EASTL-first, allocator-aware).
applyTo: "**/*.{h,hpp,cpp,cc,cxx,inl},**/*.md"
last_reviewed: 2026-04-29
reviewer: Insight Engagement Lead
---

# C++ Snippets — EA Game-Engine Conventions

These rules apply to:

- Every `.h` / `.hpp` / `.cpp` / `.cc` / `.cxx` / `.inl` file in this workshop's
  `engine_demo` workspace.
- Every fenced ` ```cpp ` block inside a Markdown file across this repo.

If a snippet cannot follow these rules without losing didactic value, label its fence
language `text` or `pseudo` instead — do not weaken the rules.

## Language & toolchain

- **Standard**: C++20 (no C++23 features unless explicitly demonstrating a future direction
  and labeled as such in prose).
- **Compilers**: MSVC 19.38+ (Windows), GCC 13+ (Linux), Clang 17+ (macOS / Linux fallback).
- **Build**: CMake 3.28+ via `CMakePresets.json` and Ninja generator. No Make, no MSBuild
  scripting.
- **Package manager**: vcpkg manifest mode (`vcpkg.json`).
- **Test framework**: GoogleTest + GoogleMock.

## Hard rules (enforced by `npm run validate:snippets`)

1. **No exceptions.** Snippets must compile with `-fno-exceptions` / `/EHs-c-`. Forbidden:
   `try`, `catch`, `throw`, `noexcept(false)`, `std::exception` derivations.
2. **No RTTI.** Compile with `-fno-rtti` / `/GR-`. Forbidden: `dynamic_cast`, `typeid`.
3. **EASTL over std.** Use `eastl::vector`, `eastl::string`, `eastl::unique_ptr`,
   `eastl::function`, `eastl::span`, `eastl::optional`, `eastl::variant`. The `std::` versions
   appear only in interop layers explicitly labeled "interop boundary".
4. **Allocator-aware containers.** Every EASTL container takes an explicit allocator
   parameter; never rely on the default global allocator in committed snippets. The
   `engine_demo` library exposes `engine_demo::allocator&` aliases — use them.
5. **`[[nodiscard]]`** on:
   - Factory functions (`make_*`, `create_*`).
   - Functions returning a status / result / error type.
   - Any function whose only purpose is to return a value.
6. **`noexcept`** on:
   - Move constructors and move assignment operators.
   - Swap functions.
   - Destructors (implicit, but must not be marked otherwise).
7. **Smart pointers**: `eastl::unique_ptr` (preferred), `eastl::shared_ptr` (only for shared
   lifetimes; document why). Never raw `new` / `delete` outside placement-new in allocator
   internals.
8. **Casts**: `static_cast`, `reinterpret_cast` (rare, document why), `const_cast` (very
   rare, document why). No C-style casts. No `dynamic_cast` (RTTI disabled).
9. **Headers**: include only what you use; forward-declare in headers when possible. Never
   `using namespace eastl;` (or `std;`) in a header. In source files it is acceptable but
   discouraged.
10. **Naming**: `PascalCase` for types, `snake_case` for functions and variables,
    `kSomething` for constants, `m_member` for non-public members. Match `engine_demo` style
    in any cross-file snippet.
11. **`constexpr`** wherever feasible. `constinit` for global state with non-trivial init.
12. **Pointers vs. references**: prefer references for non-owning, non-null inputs;
    `eastl::optional<T&>` does not exist — use a raw pointer for nullable non-owning.
13. **Determinism**: simulation snippets must use deterministic RNG (seeded `eastl::mt19937`
    or the engine's `engine_demo::rng`). Never `std::random_device` in a sim.

## Idioms to prefer

- Range-for over indexed loops when iterating containers.
- Structured bindings for tuple/pair returns.
- `if constexpr` over SFINAE for compile-time branches.
- `eastl::span<T>` for non-owning array views in function signatures.
- Concepts (C++20) for template constraints; document the concept's intent.

## Idioms to avoid (Copilot drift hot-list)

These are the patterns Copilot most often emits incorrectly. Reviewers should grep for them:

- `std::vector`, `std::string`, `std::unique_ptr`, `std::map` — flag and rewrite to EASTL.
- `using namespace std;` — flag immediately.
- `try` / `catch` / `throw` — flag and replace with status returns or `eastl::expected`.
- `dynamic_cast<T*>` — flag and replace with a typed-handle / variant pattern.
- C-style cast `(T)x` — flag and rewrite to `static_cast<T>(x)`.
- Raw `new` / `delete` — flag and rewrite to `eastl::make_unique<T>(...)`.
- `std::bind` — flag and rewrite as a lambda.
- `typedef` — flag and rewrite as `using`.
- `#include <bits/stdc++.h>` — never legal here.
- `float` for simulation precision — use `double` (or fixed-point types where indicated).

## Example skeleton

> The fence info-string `cpp no-compile` tells `tools/extract-snippets.mjs` to skip standalone
> compilation for blocks that depend on EASTL or project headers not vendored in the snippet
> harness. Use it sparingly — only for illustrative blocks. Self-contained blocks must use
> plain ` ```cpp `.

```cpp no-compile
// Good: nodiscard, noexcept move, eastl-only, allocator-aware, no exceptions.
#pragma once

#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include "engine_demo/allocator.h"

namespace engine_demo {

class [[nodiscard]] frame_budget {
public:
    explicit frame_budget(allocator& alloc) noexcept;

    frame_budget(frame_budget&&) noexcept = default;
    frame_budget& operator=(frame_budget&&) noexcept = default;

    frame_budget(const frame_budget&) = delete;
    frame_budget& operator=(const frame_budget&) = delete;

    [[nodiscard]] bool try_reserve(double milliseconds) noexcept;

private:
    allocator* m_alloc;
    eastl::vector<double, eastl_allocator_ref> m_samples;
};

} // namespace engine_demo
```
