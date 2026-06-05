// Fixed-step game loop with accumulator.
//
// Constitutional articles satisfied:
//   - 5 (determinism — note SEEDED DEFECT)
//   - 6 (real-time)
//
// SEEDED DEFECT BUG-002: see src/engine_demo/sim/game_loop.cpp.

#pragma once

#include <cstddef>
#include <cstdint>

namespace engine_demo::sim {

struct game_loop_config {
    double fixed_step_seconds{1.0 / 60.0};
    double max_substeps{8.0};
};

class [[nodiscard]] game_loop {
   public:
    explicit game_loop(game_loop_config cfg) noexcept;

    // Advance the simulation by `delta_seconds`. Returns the number of fixed-step
    // substeps taken. Pure function over (cfg, accumulator) — no global state.
    [[nodiscard]] std::uint32_t advance(double delta_seconds) noexcept;

    [[nodiscard]] double accumulator_seconds() const noexcept;

   private:
    game_loop_config m_cfg;
    // BUG-002 anchor: this is intentionally `float` rather than `double`.
    float m_accumulator_seconds{0.0f};
};

}  // namespace engine_demo::sim
