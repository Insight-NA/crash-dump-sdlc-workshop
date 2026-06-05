// GoogleTest for engine_demo::sim::game_loop.

#include "engine_demo/sim/game_loop.h"

#include <gtest/gtest.h>

namespace {

TEST(game_loop, single_step_at_exact_period) {
    engine_demo::sim::game_loop gl{{1.0 / 60.0, 8.0}};
    EXPECT_EQ(gl.advance(1.0 / 60.0), 1u);
}

TEST(game_loop, no_steps_below_period) {
    engine_demo::sim::game_loop gl{{1.0 / 60.0, 8.0}};
    EXPECT_EQ(gl.advance(1.0 / 120.0), 0u);
}

// REGRESSION TEST for BUG-002 (drift). Expected to fail on the seeded float accumulator.
TEST(game_loop, DISABLED_long_run_does_not_drift) {
    engine_demo::sim::game_loop gl{{1.0 / 60.0, 8.0}};
    constexpr int kFrames = 3600;  // 60 simulated seconds
    for (int i = 0; i < kFrames; ++i) {
        const std::uint32_t s = gl.advance(1.0 / 60.0);
        EXPECT_EQ(s, 1u) << "drift at frame " << i;
    }
}

}  // namespace
