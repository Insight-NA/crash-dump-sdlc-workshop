// Constraint solver.

#include "engine_demo/physics/constraint.h"

#include <EASTL/algorithm.h>
#include <cmath>

namespace engine_demo::physics {

constraint_solver::constraint_solver(allocator& alloc) noexcept
    : m_alloc{alloc}, m_bodies{m_alloc}, m_constraints{m_alloc} {}

void constraint_solver::add_body(body b) noexcept {
    m_bodies[b.id] = b;
}

void constraint_solver::add_constraint(distance_constraint c) noexcept {
    // Normalise direction so (a,b) is always stored with a <= b.
    if (c.a > c.b) {
        eastl::swap(c.a, c.b);
    }
    // Insert in sorted order by (a, b) to guarantee deterministic solve iteration.
    auto pos = eastl::lower_bound(
        m_constraints.begin(), m_constraints.end(), c,
        [](const distance_constraint& lhs, const distance_constraint& rhs) noexcept {
            return lhs.a < rhs.a || (lhs.a == rhs.a && lhs.b < rhs.b);
        });
    m_constraints.insert(pos, c);
}

const body* constraint_solver::try_get_body(std::uint64_t id) const noexcept {
    auto it = m_bodies.find(id);
    if (it == m_bodies.end()) {
        return nullptr;
    }
    return &it->second;
}

std::uint32_t constraint_solver::solve(std::uint32_t max_iterations) noexcept {
    for (std::uint32_t iter = 0; iter < max_iterations; ++iter) {
        for (auto& c : m_constraints) {
            auto it_a = m_bodies.find(c.a);
            auto it_b = m_bodies.find(c.b);
            if (it_a == m_bodies.end() || it_b == m_bodies.end())
                continue;

            body& a = it_a->second;
            body& b_ref = it_b->second;

            const double dx = b_ref.position_x - a.position_x;
            const double dy = b_ref.position_y - a.position_y;
            const double dz = b_ref.position_z - a.position_z;
            const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length < 1.0e-12)
                continue;

            const double diff = (length - c.rest_length) / length;
            const double w_sum = a.inverse_mass + b_ref.inverse_mass;
            if (w_sum < 1.0e-12)
                continue;

            const double k_a = a.inverse_mass / w_sum;
            const double k_b = b_ref.inverse_mass / w_sum;
            a.position_x += k_a * dx * diff;
            a.position_y += k_a * dy * diff;
            a.position_z += k_a * dz * diff;
            b_ref.position_x -= k_b * dx * diff;
            b_ref.position_y -= k_b * dy * diff;
            b_ref.position_z -= k_b * dz * diff;
        }
    }
    return max_iterations;
}

}  // namespace engine_demo::physics
