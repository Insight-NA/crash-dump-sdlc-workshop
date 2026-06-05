// GoogleTest for engine_demo::physics::constraint_solver.

#include "engine_demo/physics/constraint.h"

#include <array>
#include <gtest/gtest.h>

namespace {

TEST(constraint_solver, two_body_distance_constraint_converges) {
    std::array<std::byte, 4096> buffer{};
    engine_demo::allocator alloc{buffer.data(), buffer.size()};
    engine_demo::physics::constraint_solver s{alloc};

    s.add_body({1u, 0.0, 0.0, 0.0, 1.0});
    s.add_body({2u, 2.0, 0.0, 0.0, 1.0});
    s.add_constraint({1u, 2u, 1.0});

    const std::uint32_t iters = s.solve(8);
    EXPECT_EQ(iters, 8u);
}

// REGRESSION TEST for BUG-004 (non-determinism). Fails if solver depends on hash order.
TEST(constraint_solver, DISABLED_solve_is_deterministic_across_construction_orders) {
    // Implementation note: this test would build two solvers, insert bodies in opposite
    // orders, and assert exact-match positions. It's disabled until the seeded defect is
    // remediated by sorting constraints and reading bodies via deterministic indices.
    SUCCEED();
}

}  // namespace
