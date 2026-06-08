# Exercise 01 — Pinpoint Phase 1 unaided

**Time:** 10 min · **Difficulty:** intermediate

## Goal

Cold-load the BUG-001 dump and use Copilot in Plan mode to identify the exact line inside
`allocator::allocate()` responsible for an out-of-bounds write (CWE-787). The crash fires
on the **third** 40-byte allocation into a 160-byte arena: 32-byte alignment padding
silently pushes the computed end-address past the buffer tail, overwriting the adjacent
guard page. Your target is under 5 minutes.

## Setup

You need the crash dump captured during pre-work (`BUG-001.dmp.local` on Windows,
`.core.local` on Linux/macOS).

If you skipped the pre-work, capture it now:

```bash
# Linux / macOS
cd fixtures/crash_dumps/BUG-001
./capture.sh
```

```powershell
# Windows
cd fixtures\crash_dumps\BUG-001
.\capture.ps1
```

The dump and symbols land next to `repro.cpp`.

## Steps

1. Open the dump in VS Code's debug view.
2. Add these three files to your Copilot context belt:
   - `fixtures/crash_dumps/BUG-001/repro.cpp`
   - `include/engine_demo/allocator.h`
   - `src/engine_demo/allocator.cpp`
3. Switch Copilot Chat to **Plan mode** and paste the prompt below, then start your timer:

   ```text
   #file:fixtures/crash_dumps/BUG-001/repro.cpp
   #file:include/engine_demo/allocator.h
   #file:src/engine_demo/allocator.cpp
   A debugger reports a heap-corruption fault inside the third call to allocator::allocate.
   Identify the call stack frame most likely responsible for the corruption. Cite the line.
   ```

   > **Recovery — if Copilot blames the caller:** redirect with
   > `The crash is INSIDE allocator::allocate, not the caller. Re-pinpoint the exact line.`

4. Stop the timer when Copilot has cited the offending line.

## Acceptance

Copilot identifies the line inside `allocate()` where `m_offset` is advanced using the
aligned value **before** a valid bounds check against `m_capacity`. Because alignment
padding is not accounted for in the guard, the third 40-byte/32-byte-aligned allocation
into the 160-byte arena can return a pointer past the buffer tail — overwriting the guard
page and crashing.

## Reflection

If you exceeded 5 minutes, what was missing from your belt?

- **No repro file** — without `repro.cpp`, Copilot has no anchor for "the third call";
  it cannot reason about why two allocations succeed but the third does not.
- **No allocator source** — without `allocator.cpp`, Copilot cannot see the `align_up`
  call order and identify that the bounds check runs on the wrong (pre-alignment) value.

The alignment arithmetic is the key clue: the check must happen _after_ the aligned
offset is computed, not before.
