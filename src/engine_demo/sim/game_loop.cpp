// Fixed-step game loop.
//
// SEEDED DEFECT BUG-002 (Session 03 anchor):
//   The accumulator is `float`; the constitutional article 5 mandate is `double`. With
//   1/60s steps and 1/300s sub-steps, the float accumulator drifts after ~30 seconds of
//   simulated time, causing the substep count to oscillate. Replay diverges from a
//   reference run captured with a double accumulator.
//
// Do NOT fix as part of the demo — Session 03 fixes it on stage.

#include "engine_demo/sim/game_loop.h"

namespace engine_demo::sim {

game_loop::game_loop(game_loop_config cfg) noexcept : m_cfg{cfg} {}

std::uint32_t game_loop::advance(double delta_seconds) noexcept {
    // BUG-002: implicit narrowing from double to float on every accumulate; over many
    // small dt values this accumulates representation error at ~ulp(float)/step.
    m_accumulator_seconds += static_cast<float>(delta_seconds);

    std::uint32_t substeps = 0;
    while (static_cast<double>(m_accumulator_seconds) >= m_cfg.fixed_step_seconds &&
           static_cast<double>(substeps) < m_cfg.max_substeps) {
        m_accumulator_seconds -= static_cast<float>(m_cfg.fixed_step_seconds);
        ++substeps;
    }
    return substeps;
}

double game_loop::accumulator_seconds() const noexcept {
    return static_cast<double>(m_accumulator_seconds);
}

}  // namespace engine_demo::sim
