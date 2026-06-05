# `apps/sandbox/ea-sandbox`

Graphical demo for the EA × Insight Copilot workshop. A multi-scene 2D physics
showcase driven by the `engine_demo` library and rendered with raylib.

Four scenes selectable at runtime:

| #   | Scene          | What it shows                                                            |
| --- | -------------- | ------------------------------------------------------------------------ |
| 1   | rope           | 24-node verlet chain + 32 free particles (the original demo)             |
| 2   | pendulum tower | 4 chains of length 16 / 12 / 18 / 10, anchored across the top            |
| 3   | cloth          | 12 × 8 grid with structural + alternating shear constraints (~250 edges) |
| 4   | particle storm | 500 free particles orbiting two gravity wells                            |

This is the **cold-open** demo for Session 01 and the **canonical bug target**
for Sessions 02–04: every committed seeded bug surfaces visually here.

## Build

> **Prerequisite — `VCPKG_ROOT`:** vcpkg must be available and `$VCPKG_ROOT` must point to it.
> If you cloned vcpkg to `~/vcpkg`:
>
> ```bash
> export VCPKG_ROOT="$HOME/vcpkg"
> ```
>
> vcpkg must be a **full clone** (not `--depth 1`) so the manifest's `builtin-baseline`
> commit resolves. If you cloned shallow, run `git fetch --unshallow` inside `$VCPKG_ROOT`.

### macOS (Apple Silicon) — push-button script

From the repo root, run the helper script — it handles `VCPKG_ROOT` detection,
configure (skipped if `build/CMakeCache.txt` already exists), build, and launch:

```bash
# Interactive (default: rope scene, seed 42)
./run_sandbox.sh

# Pick a scene or seed
./run_sandbox.sh --scene cloth
./run_sandbox.sh --scene storm --seed 7

# Headless / CI
./run_sandbox.sh --headless --seed 42 --frames 60
```

The script uses `open … --args` for interactive mode (gives the process a proper foreground
GUI activation so macOS doesn't ANR-flag it) and invokes the binary directly for
headless/screenshot mode.

### macOS (Apple Silicon) — manual steps

Use the `macos-arm64` preset — it sets the `arm64-osx` vcpkg triplet and
`CMAKE_OSX_ARCHITECTURES=arm64`:

```bash
export VCPKG_ROOT="$HOME/vcpkg"
cmake --preset macos-arm64          # configures into build/
cmake --build build --target ea-sandbox
open build/apps/sandbox/ea-sandbox.app --args --seed 42
```

For headless / screenshot / scripting, invoke the binary inside the bundle directly:

```bash
./build/apps/sandbox/ea-sandbox.app/Contents/MacOS/ea-sandbox \
    --headless --seed 42 --frames 60 --out trace.csv
```

### Linux / Windows

```bash
export VCPKG_ROOT="$HOME/vcpkg"   # or set VCPKG_ROOT in your environment
cmake --preset default-debug
cmake --build --preset default-debug
./build/apps/sandbox/ea-sandbox --seed 42
```

To skip the sandbox (and the raylib dependency):

```bash
cmake --preset default-debug -DENGINE_DEMO_BUILD_SANDBOX=OFF
```

On Linux you need raylib's system deps before vcpkg builds raylib:

```bash
sudo apt-get install -y libgl1-mesa-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libasound2-dev
```

On macOS the sandbox links the system Cocoa / IOKit / CoreVideo / CoreAudio /
OpenGL frameworks plus `glfw3` (vcpkg-provided). All of that is wired in
`apps/sandbox/CMakeLists.txt` under an `if(APPLE)` block — no manual setup.

## Run

```text
ea-sandbox                                              # interactive, scene 1 (rope)
ea-sandbox --scene cloth                                # interactive, start in cloth
ea-sandbox --scene storm                                # interactive, particle storm
ea-sandbox --headless --seed 42 --frames 600 \
           --out trace.csv [--scene rope|pendulum|cloth|storm]
ea-sandbox --seed 42 --scene cloth \
           --screenshot demo.png --warmup 240          # capture a PNG of the scene
ea-sandbox --help
```

### Interactive controls

| Key                   | Action                                                        |
| --------------------- | ------------------------------------------------------------- |
| `1` / `2` / `3` / `4` | Switch scene (rope / pendulum tower / cloth / particle storm) |
| `Space`               | Pause / resume the simulation                                 |
| `S`                   | Single-step one fixed step (only meaningful when paused)      |
| `R`                   | Reseed without changing the seed input — visualizes BUG-004   |
| `H`                   | Toggle the HUD (clean capture mode)                           |
| `T`                   | Toggle particle motion trails                                 |
| `+` / `-`             | Speed up / slow down simulation time                          |
| `LMB` drag            | Grab + drag the nearest dynamic rope/cloth node               |
| `RMB`                 | Spawn a 12-particle burst at the cursor                       |
| `F1`                  | Deliberate null-deref crash (Session 02 crash-dump demo)      |
| `Esc`                 | Quit                                                          |

### Bug-visibility cheat sheet

| Anchor  | Where it surfaces in the HUD                                            |
| ------- | ----------------------------------------------------------------------- |
| BUG-002 | `wall=… sim=… drift=±…ms` line (game_loop float accumulator drift)      |
| BUG-004 | `trace_digest=…` line; press `R` and watch it diverge despite same seed |
| BUG-006 | `frame_avg=…ms` rolling-window readout (frame_budget seeded defect)     |

### Determinism digest matrix

Headless seed=42, 60 fixed-step frames (verified locally on macOS / AppleClang 17):

| Scene          | `trace_digest` (final state) |
| -------------- | ---------------------------- |
| rope           | `33a6319d856d4869`           |
| pendulum tower | `b0b37e5a36eafa6a`           |
| cloth          | `ef6340b55f1b9edc`           |
| particle storm | `968ce23fc350cfaf`           |

The rope digest is the canonical CI golden-trace value referenced by
`.github/workflows/ci-build-demo-workspace.yml`. The other three are
recorded for future regression assertions and to demonstrate that
determinism is a per-scene contract, not a single-scene property.

## Debug telemetry

The sandbox ships full-debug runtime telemetry — frame-graph spans, window /
input / OS events, physics state, render counters, raylib trace-log mirror,
crash signal handlers, and a hang watchdog — all routed through one JSONL
sink plus an always-on stderr 1-line liveness tick. Headless mode is
unaffected (CSV digest stays byte-stable).

```bash
# Frame-level telemetry to a JSONL file + watchdog + stderr ticks:
./build/apps/sandbox/ea-sandbox.app/Contents/MacOS/ea-sandbox \
    --debug --telemetry-out /tmp/sandbox.jsonl --watchdog --seed 42

# Full verbose stream (adds physics/render records per frame):
./build/apps/sandbox/ea-sandbox.app/Contents/MacOS/ea-sandbox \
    --telemetry-level verbose --telemetry-out /tmp/sandbox.jsonl --seed 42
```

Stderr tick (always on, ~1 Hz):

```text
[T+  1.3s] f=60 fps=60.0 p50=16.62ms p95=16.83ms hb=60
```

JSONL record kinds:

| `kind`     | When                        | Notable fields                                                                                                                                                  |
| ---------- | --------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `boot`     | startup (level ≥ frame)     | `level`, `watchdog`                                                                                                                                             |
| `frame`    | every frame (level ≥ frame) | `t_input_us`, `t_step_us`, `t_draw_world_us`, `t_draw_hud_us`, `t_swap_us`, `t_sleep_us`, `t_total_us`, `fps`, `vp_w`, `vp_h`, `focused`, `minimized`, `hidden` |
| `physics`  | every frame (verbose)       | `bodies`, `constraints`, `particles`, `paused`, `scene`, `seed`, `state_digest`, `substeps`, `sim_time`                                                         |
| `render`   | every frame (verbose)       | `world_calls`, `hud_real_refreshes`, `hud_blits`, `draw_text_calls`                                                                                             |
| `event`    | transitions / inputs        | `category` ∈ {`window`, `input`, `scene`, `loop`, `raylib`, `watchdog`, `crash`, `boot`}                                                                        |
| `shutdown` | clean exit                  | `t_us`                                                                                                                                                          |

Common triage queries with `jq`:

```bash
# Frame-time outliers (> 25 ms total):
jq -c 'select(.kind=="frame" and .t_total_us > 25000)' /tmp/sandbox.jsonl

# All window/focus transitions:
jq -c 'select(.kind=="event" and .category=="window")' /tmp/sandbox.jsonl

# Reseed events with pre/post digests (BUG-004 visualizer):
jq -c 'select(.kind=="event" and .msg=="reseed")' /tmp/sandbox.jsonl

# raylib WARN/ERROR lines:
jq -c 'select(.kind=="event" and .category=="raylib" and .level!="info")' /tmp/sandbox.jsonl

# Watchdog hangs:
jq -c 'select(.kind=="event" and .category=="watchdog")' /tmp/sandbox.jsonl
```

Crash handlers install `SIGSEGV / SIGBUS / SIGABRT / SIGFPE / SIGILL` by
default; pass `--no-crash-handlers` when running under a debugger so the
debugger receives the signal first. The watchdog observes a per-frame
heartbeat and emits a `watchdog` event after 2 s of stall — it never
kills the process.

## Constitutional conformance

- raylib is consumed only inside `app.cpp` — labeled **interop boundary** per
  [`.github/instructions/cpp-snippets.instructions.md`](../../.github/instructions/cpp-snippets.instructions.md)
  §3.
- All sandbox-internal containers stay EASTL with `engine_demo::eastl_allocator_ref`.
- No exceptions, no RTTI; argv parsing returns a status enum and uses
  `eastl::string_view`.
- Determinism: scene initialization is seeded via `engine_demo::sim::rng`; the
  headless mode emits a per-frame `state_digest` and a final 64-bit FNV-1a digest
  for golden-trace assertions. Particle trails and mouse-driven interactions
  are render-only and never feed `state_digest`.
