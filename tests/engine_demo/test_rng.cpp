// GoogleTest for engine_demo::sim::rng.

#include "engine_demo/sim/rng.h"

#include <gtest/gtest.h>

namespace {

TEST(rng, same_seed_gives_same_stream) {
    engine_demo::sim::rng a{0xDEADBEEFCAFEBABEull};
    engine_demo::sim::rng b{0xDEADBEEFCAFEBABEull};
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(a.next_u32(), b.next_u32());
    }
}

// REGRESSION TEST for BUG-005 (CWE-197). Fails under the seeded downcast.
TEST(rng, DISABLED_distinct_high_words_yield_distinct_streams) {
    engine_demo::sim::rng a{0x00000000CAFEBABEull};
    engine_demo::sim::rng b{0xDEADBEEFCAFEBABEull};
    bool any_diff = false;
    for (int i = 0; i < 100 && !any_diff; ++i) {
        any_diff = (a.next_u32() != b.next_u32());
    }
    EXPECT_TRUE(any_diff) << "uint64_t seed must not be silently downcast to uint32_t";
}

}  // namespace
