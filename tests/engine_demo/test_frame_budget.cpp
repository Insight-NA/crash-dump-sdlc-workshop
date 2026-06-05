// GoogleTest for engine_demo::frame_budget.

#include "engine_demo/frame_budget.h"

#include <array>
#include <gtest/gtest.h>

namespace {

TEST(frame_budget, single_sample_returns_that_sample) {
    std::array<std::byte, 1024> buffer{};
    engine_demo::allocator alloc{buffer.data(), buffer.size()};
    engine_demo::frame_budget fb{alloc, 16};
    fb.record_sample(5.0);
    EXPECT_NEAR(fb.rolling_average(), 5.0, 1.0e-9);
}

TEST(frame_budget, average_over_two_samples) {
    std::array<std::byte, 1024> buffer{};
    engine_demo::allocator alloc{buffer.data(), buffer.size()};
    engine_demo::frame_budget fb{alloc, 16};
    fb.record_sample(2.0);
    fb.record_sample(4.0);
    EXPECT_NEAR(fb.rolling_average(), 3.0, 1.0e-9);
}

// REGRESSION TEST for BUG-006 (currently fails on warm-up — intentional during demo).
TEST(frame_budget, DISABLED_first_sample_is_not_double_counted_on_warmup) {
    std::array<std::byte, 1024> buffer{};
    engine_demo::allocator alloc{buffer.data(), buffer.size()};
    engine_demo::frame_budget fb{alloc, 16};
    fb.record_sample(10.0);
    EXPECT_EQ(fb.sample_count(), 1u);
    EXPECT_NEAR(fb.rolling_average(), 10.0, 1.0e-9);
}

}  // namespace
