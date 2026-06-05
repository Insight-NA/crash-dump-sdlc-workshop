// Headless mode — runs the scene N fixed-step ticks, writes per-frame body
// positions to a CSV file, and computes a deterministic 64-bit FNV-1a digest
// over the final state. CI uses this digest to assert determinism on the
// canonical (BUG-fixed) branch and to demonstrate divergence on the seeded
// branch (BUG-004).

#pragma once

#include "scene.h"

#include <cstdint>

namespace ea_sandbox {

enum class headless_status : std::uint8_t {
    ok = 0,
    invalid_arguments = 1,
    cannot_open_output = 2,
    write_failed = 3,
};

struct headless_result {
    headless_status status{headless_status::ok};
    std::uint64_t digest{0};
    std::uint32_t frames_written{0};
};

[[nodiscard]] headless_result run_headless(std::uint64_t seed,
                                           std::uint32_t frames,
                                           const char* out_path,
                                           scene_kind kind = scene_kind::rope) noexcept;

}  // namespace ea_sandbox
