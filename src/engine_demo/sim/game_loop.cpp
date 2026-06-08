// Fixed-step game loop.

#include "engine_demo/sim/game_loop.h"

namespace engine_demo::sim {

game_loop::game_loop(game_loop_config cfg) noexcept : m_cfg{cfg} {}

std::uint32_t game_loop::advance(double delta_seconds) noexcept {
    m_accumulator_seconds += delta_seconds;

    std::uint32_t substeps = 0;
    while (m_accumulator_seconds >= m_cfg.fixed_step_seconds &&
           static_cast<double>(substeps) < m_cfg.max_substeps) {
        m_accumulator_seconds -= m_cfg.fixed_step_seconds;
        ++substeps;
    }
    return substeps;
}

double game_loop::accumulator_seconds() const noexcept {
    return m_accumulator_seconds;
}

}  // namespace engine_demo::sim
