// Interactive sandbox app — raylib window + HUD.
//
// raylib is the rendering interop boundary; this translation unit is the only
// place we touch raylib's C API. All sandbox-internal containers stay EASTL +
// allocator-aware.

#pragma once

#include "scene.h"

#include <cstdint>

namespace ea_sandbox {

// Open a raylib window and run the scene to completion (Esc to quit).
// Returns 0 on clean exit, non-zero on initialization failure.
//
// If `screenshot_path` is non-null, runs `screenshot_warmup_frames` frames,
// writes a PNG screenshot to that path, and exits cleanly. Used for
// demo capture and visual smoke tests.
[[nodiscard]] int run_interactive(std::uint64_t initial_seed,
                                  scene_kind initial_scene = scene_kind::rope,
                                  const char* screenshot_path = nullptr,
                                  int screenshot_warmup_frames = 120) noexcept;
}  // namespace ea_sandbox
