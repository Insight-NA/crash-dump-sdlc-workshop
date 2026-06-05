# engine_demo Constitution (Spec-Kit ground truth)

Eight non-negotiable articles that bind every `/specify`, `/plan`, `/tasks`, and `/implement`
invocation in this workspace. The reviewer chat mode rejects any artifact that violates them.

## Article 1 — No exceptions

Code compiles with `-fno-exceptions` / `/EHs-c-`. No `try`, `catch`, `throw`,
`noexcept(false)`, or `std::exception` derivations. Fallible operations return a status
enum (preferred) or `eastl::expected<T, status>` where the EASTL revision supports it.

## Article 2 — No RTTI

Code compiles with `-fno-rtti` / `/GR-`. No `dynamic_cast` or `typeid`. Type discrimination
uses tagged unions, `eastl::variant`, or typed-handle patterns.

## Article 3 — EASTL-first

EASTL containers and smart pointers are the default in committed code (`eastl::vector`,
`eastl::string`, `eastl::unique_ptr`, `eastl::shared_ptr`, `eastl::function`, `eastl::span`,
`eastl::optional`, `eastl::variant`). `std::` containers are permitted only in interop layers
explicitly labeled `// interop boundary` in code comments.

## Article 4 — Allocator-aware containers

Every EASTL container takes an explicit allocator at construction. The project's
`engine_demo::allocator` (in `include/engine_demo/allocator.h`) is the source of truth.
Default-constructed containers are forbidden in committed code.

## Article 5 — Determinism

Simulation paths are deterministic across runs at fixed seeds:

- `eastl::mt19937` seeded explicitly at construction; `std::random_device` is forbidden in
  sim paths.
- Time accumulators are `double`. `float` is permitted only at the render boundary.
- Iteration order over containers is deterministic; never `eastl::unordered_map` keyed on
  pointer identity in sim paths.

## Article 6 — Real-time budgets

Frame budgets: ≤16.67 ms at 60 FPS, ≤8.33 ms at 120 FPS. No allocation in inner loops; pool
or arena up front. Branch-prediction-friendly idioms preferred. No locks in render thread;
use lockless ring buffers for cross-thread handoff.

## Article 7 — Test-first

Every public function has at least one GTest covering happy + edge cases. `/implement` tasks
emit the test before the implementation. CI gates on green tests + clang-tidy clean.

## Article 8 — HITL gates

Spec-Kit pauses for human approval **between** `/plan` and `/tasks`, and **between every
`/implement` task**. Coding Agent handoffs require human review of the produced PR before
merge. The `reviewer` chat mode runs over every session bundle before delivery.

## Enforcement

- **clang-tidy** (`.clang-tidy`) flags `bugprone-*`, `cert-*`, `cppcoreguidelines-*`,
  `modernize-*`, `performance-*`, `portability-*`, `readability-*`. WarningsAsErrors mirrors.
- **CI** runs `cmake --preset default-debug && cmake --build && ctest --output-on-failure`
  on Ubuntu and Windows on every PR.
- **Reviewer chat mode** (`/.github/chatmodes/reviewer.chatmode.md` at the build-system root)
  produces a severity report; Critical findings block merge.
