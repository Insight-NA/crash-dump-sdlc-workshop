# engine_demo Copilot instructions (workshop demo workspace)

> **Stub.** Fleshed out by `.github/prompts/demo-workspace-instructions-prompts-chatmodes.prompt.md`.

This is the workspace EA engineers learn *from*. Copilot operating inside
this repository must:

- Use **EASTL** containers, never `std::` (interop boundaries excepted, labeled as such).
- Compile under **`-fno-exceptions -fno-rtti`** — no `try`/`catch`, no `dynamic_cast`,
  no `typeid`.
- Take an explicit **allocator** parameter on every container.
- Use **`[[nodiscard]]`** on factories and status-returning functions.
- Use **`noexcept`** on moves and swaps.
- Use **`double`** for simulation accumulators; **`eastl::mt19937`** seeded explicitly for sim RNG.
- Honor **[`specs/constitution.md`](specs/constitution.md)** as ground truth.

When suggesting code, cite the constitutional article you're satisfying when relevant.
