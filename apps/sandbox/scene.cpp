#include "scene.h"

#include <cmath>
#include <cstdlib>
#include <new>

namespace ea_sandbox {

namespace {

[[nodiscard]] std::uint64_t fnv1a_mix(std::uint64_t hash, double v) noexcept {
    // Hash a double's IEEE-754 bit pattern. Reinterpret via memcpy-equivalent.
    union {
        double d;
        std::uint64_t u;
    } pun{};
    pun.d = v;
    std::uint64_t bits = pun.u;
    for (int i = 0; i < 8; ++i) {
        const std::uint8_t b = static_cast<std::uint8_t>(bits & 0xFFu);
        hash ^= static_cast<std::uint64_t>(b);
        hash *= 0x100000001b3ull;  // FNV-1a 64-bit prime
        bits >>= 8;
    }
    return hash;
}

}  // namespace

const char* scene_kind_name(scene_kind k) noexcept {
    switch (k) {
        case scene_kind::rope:
            return "rope";
        case scene_kind::pendulum_tower:
            return "pendulum tower";
        case scene_kind::cloth:
            return "cloth";
        case scene_kind::particle_storm:
            return "particle storm";
    }
    return "unknown";
}

scene::scene(std::uint64_t seed)
    : m_arena_buffer{new unsigned char[kArenaBytes]()},
      m_alloc{m_arena_buffer, kArenaBytes},
      m_alloc_ref{m_alloc},
      m_seed{seed},
      m_rng{seed},
      m_loop{engine_demo::sim::game_loop_config{kFixedStepSeconds, 8.0}},
      m_solver{m_alloc},
      m_budget{m_alloc, 64},
      m_body_ids{m_alloc_ref},
      m_verlet{m_alloc_ref},
      m_anchor_flags{m_alloc_ref},
      m_edges{m_alloc_ref},
      m_particles{m_alloc_ref},
      m_trails{m_alloc_ref} {
    rebuild_scene(seed);
}

scene::~scene() {
    delete[] m_arena_buffer;
}

void scene::reseed(std::uint64_t seed) noexcept {
    m_seed = seed;
    m_rng = engine_demo::sim::rng{seed};
    m_sim_time = 0.0;
    m_wall_time = 0.0;
    m_total_substeps = 0;
    m_held_index = static_cast<std::size_t>(-1);
    m_trail_head = 0;
    rebuild_scene(seed);
}

void scene::switch_scene(scene_kind kind) noexcept {
    m_kind = kind;
    m_rng = engine_demo::sim::rng{m_seed};
    m_sim_time = 0.0;
    m_wall_time = 0.0;
    m_total_substeps = 0;
    m_held_index = static_cast<std::size_t>(-1);
    m_trail_head = 0;
    rebuild_scene(m_seed);
}

void scene::reset_solver() noexcept {
    // Rebuild a fresh solver in-place (clears m_bodies + m_constraints).
    m_solver.~constraint_solver();
    new (&m_solver) engine_demo::physics::constraint_solver{m_alloc};
}

void scene::add_pinned_body(double x, double y) noexcept {
    engine_demo::physics::body b{};
    b.id = static_cast<std::uint64_t>(m_body_ids.size() + 1);
    b.position_x = x;
    b.position_y = y;
    b.position_z = 0.0;
    b.inverse_mass = 0.0;
    m_solver.add_body(b);
    m_body_ids.push_back(b.id);
    m_verlet.push_back(verlet_state{x, y});
    m_anchor_flags.push_back(static_cast<std::uint8_t>(1));
}

void scene::add_dynamic_body(double x, double y) noexcept {
    engine_demo::physics::body b{};
    b.id = static_cast<std::uint64_t>(m_body_ids.size() + 1);
    b.position_x = x;
    b.position_y = y;
    b.position_z = 0.0;
    b.inverse_mass = 1.0;
    m_solver.add_body(b);
    m_body_ids.push_back(b.id);
    m_verlet.push_back(verlet_state{x, y});
    m_anchor_flags.push_back(static_cast<std::uint8_t>(0));
}

void scene::add_edge(std::size_t a, std::size_t b, double rest) noexcept {
    engine_demo::physics::distance_constraint c{};
    c.a = m_body_ids[a];
    c.b = m_body_ids[b];
    c.rest_length = rest;
    m_solver.add_constraint(c);
    m_edges.push_back(a);
    m_edges.push_back(b);
}

bool scene::is_anchor(std::size_t i) const noexcept {
    return i < m_anchor_flags.size() && m_anchor_flags[i] != 0;
}

void scene::rebuild_scene(std::uint64_t /*seed*/) noexcept {
    m_body_ids.clear();
    m_verlet.clear();
    m_anchor_flags.clear();
    m_edges.clear();
    m_particles.clear();
    m_trails.clear();

    reset_solver();

    switch (m_kind) {
        case scene_kind::rope:
            build_rope();
            break;
        case scene_kind::pendulum_tower:
            build_pendulum_tower();
            break;
        case scene_kind::cloth:
            build_cloth();
            break;
        case scene_kind::particle_storm:
            build_particle_storm();
            break;
    }

    // Initialize trails to current particle positions so the first frame's
    // trail render is a degenerate point rather than streaking from origin.
    m_trails.resize(m_particles.size() * kTrailLength);
    for (std::size_t p = 0; p < m_particles.size(); ++p) {
        const trail_point t{static_cast<float>(m_particles[p].x),
                            static_cast<float>(m_particles[p].y)};
        for (std::size_t i = 0; i < kTrailLength; ++i) {
            m_trails[p * kTrailLength + i] = t;
        }
    }
}

void scene::build_rope() noexcept {
    // ---- Preserved byte-for-byte from the original demo so the headless
    // rope digest (seed=42 -> 33a6319d856d4869) stays stable. Do not
    // change body order, RNG draw order, or initial-position math. ----
    const double rest_length = 0.30;
    const double anchor_x = 0.0;
    const double anchor_y = 3.0;

    for (std::size_t i = 0; i < kRopeNodes; ++i) {
        const double angle = 0.61;  // ~35 degrees in radians
        const double dx = std::sin(angle) * rest_length;
        const double dy = -std::cos(angle) * rest_length;
        engine_demo::physics::body b{};
        b.id = static_cast<std::uint64_t>(i + 1);
        b.position_x = anchor_x + dx * static_cast<double>(i);
        b.position_y = anchor_y + dy * static_cast<double>(i);
        b.position_z = 0.0;
        b.inverse_mass = (i == 0) ? 0.0 : 1.0;
        m_solver.add_body(b);
        m_body_ids.push_back(b.id);
        m_verlet.push_back(verlet_state{b.position_x, b.position_y});
        m_anchor_flags.push_back(static_cast<std::uint8_t>(i == 0 ? 1 : 0));
    }
    for (std::size_t i = 0; i + 1 < kRopeNodes; ++i) {
        engine_demo::physics::distance_constraint c{};
        c.a = static_cast<std::uint64_t>(i + 1);
        c.b = static_cast<std::uint64_t>(i + 2);
        c.rest_length = rest_length;
        m_solver.add_constraint(c);
        m_edges.push_back(i);
        m_edges.push_back(i + 1);
    }

    for (std::size_t i = 0; i < kRopeFreeParticles; ++i) {
        particle p{};
        p.x = (m_rng.next_double_unit() * 6.0) - 3.0;
        p.y = (m_rng.next_double_unit() * 2.0) - 1.0;
        p.vx = (m_rng.next_double_unit() * 2.0) - 1.0;
        p.vy = (m_rng.next_double_unit() * 2.0) - 1.0;
        p.radius = 0.04 + m_rng.next_double_unit() * 0.04;
        m_particles.push_back(p);
    }
}

void scene::build_pendulum_tower() noexcept {
    // Four chains anchored across the top, varying lengths for visual interest.
    constexpr std::size_t kLengths[kPendulumChains] = {16, 12, 18, 10};
    constexpr double kAnchorX[kPendulumChains] = {-2.4, -0.8, 0.8, 2.4};
    const double rest_length = 0.22;
    const double anchor_y = 1.8;

    for (std::size_t c = 0; c < kPendulumChains; ++c) {
        const std::size_t len = kLengths[c];
        // Per-chain initial tilt drawn from the RNG so reseed perturbs the scene.
        const double tilt = (m_rng.next_double_unit() * 0.9 - 0.45) + (c % 2 == 0 ? 0.4 : -0.4);
        const double dx = std::sin(tilt) * rest_length;
        const double dy = -std::cos(tilt) * rest_length;

        const std::size_t base = m_body_ids.size();
        for (std::size_t i = 0; i < len; ++i) {
            const double x = kAnchorX[c] + dx * static_cast<double>(i);
            const double y = anchor_y + dy * static_cast<double>(i);
            if (i == 0) {
                add_pinned_body(x, y);
            } else {
                add_dynamic_body(x, y);
            }
        }
        for (std::size_t i = 0; i + 1 < len; ++i) {
            add_edge(base + i, base + i + 1, rest_length);
        }
    }

    // A handful of free particles for atmosphere.
    for (std::size_t i = 0; i < 24; ++i) {
        particle p{};
        p.x = (m_rng.next_double_unit() * 6.0) - 3.0;
        p.y = (m_rng.next_double_unit() * 1.5) - 1.5;
        p.vx = (m_rng.next_double_unit() * 1.6) - 0.8;
        p.vy = m_rng.next_double_unit() * 1.0;
        p.radius = 0.04 + m_rng.next_double_unit() * 0.03;
        m_particles.push_back(p);
    }
}

void scene::build_cloth() noexcept {
    // 12 x 8 grid spanning x in [-2.5, 2.5], y in [+1.5 down to -1.0]. Top row pinned.
    const double x0 = -2.5;
    const double x1 = 2.5;
    const double y_top = 1.5;
    const double y_bot = -1.0;
    const double dx = (x1 - x0) / static_cast<double>(kClothCols - 1);
    const double dy_step = (y_top - y_bot) / static_cast<double>(kClothRows - 1);
    const double rest_h = dx;
    const double rest_v = dy_step;
    const double rest_d = std::sqrt(dx * dx + dy_step * dy_step);

    auto idx = [](std::size_t r, std::size_t c) noexcept -> std::size_t {
        return r * kClothCols + c;
    };

    for (std::size_t r = 0; r < kClothRows; ++r) {
        for (std::size_t c = 0; c < kClothCols; ++c) {
            // Mild rng perturbation in y to break perfect symmetry.
            const double jitter = (m_rng.next_double_unit() - 0.5) * 0.02;
            const double x = x0 + dx * static_cast<double>(c);
            const double y = y_top - dy_step * static_cast<double>(r) + jitter;
            if (r == 0) {
                add_pinned_body(x, y);
            } else {
                add_dynamic_body(x, y);
            }
        }
    }

    // Structural constraints (horizontal + vertical) only — shear diagonals
    // would over-constrain the grid against this 8-iteration solver and
    // visually freeze it.  Slight slack in the rest length lets gravity drape
    // the cloth between anchors instead of holding it perfectly flat.
    const double slack = 1.04;
    for (std::size_t r = 0; r < kClothRows; ++r) {
        for (std::size_t c = 0; c + 1 < kClothCols; ++c) {
            add_edge(idx(r, c), idx(r, c + 1), rest_h * slack);
        }
    }
    for (std::size_t r = 0; r + 1 < kClothRows; ++r) {
        for (std::size_t c = 0; c < kClothCols; ++c) {
            add_edge(idx(r, c), idx(r + 1, c), rest_v * slack);
        }
    }
    // One shear diagonal per cell (alternating direction by parity) for
    // shape memory without over-constraint.
    for (std::size_t r = 0; r + 1 < kClothRows; ++r) {
        for (std::size_t c = 0; c + 1 < kClothCols; ++c) {
            if (((r + c) & 1u) == 0u) {
                add_edge(idx(r, c), idx(r + 1, c + 1), rest_d * slack);
            } else {
                add_edge(idx(r, c + 1), idx(r + 1, c), rest_d * slack);
            }
        }
    }
}

void scene::build_particle_storm() noexcept {
    // No rope/cloth bodies — only free particles. Two implicit gravity wells
    // drive the motion in substep().
    for (std::size_t i = 0; i < kStormParticles; ++i) {
        particle p{};
        p.x = (m_rng.next_double_unit() * 6.0) - 3.0;
        p.y = (m_rng.next_double_unit() * 4.0) - 2.0;
        const double speed = 0.5 + m_rng.next_double_unit() * 1.5;
        const double angle = m_rng.next_double_unit() * 6.2831853;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.radius = 0.025 + m_rng.next_double_unit() * 0.03;
        m_particles.push_back(p);
    }
}

bool scene::grab_nearest_rope_node(double wx, double wy, double max_dist) noexcept {
    std::size_t best = static_cast<std::size_t>(-1);
    double best_d2 = max_dist * max_dist;
    for (std::size_t i = 0; i < m_body_ids.size(); ++i) {
        if (m_anchor_flags[i] != 0) {
            continue;  // can't drag pinned anchors
        }
        double x = 0.0;
        double y = 0.0;
        if (!rope_node_position(i, x, y)) {
            continue;
        }
        const double ddx = x - wx;
        const double ddy = y - wy;
        const double d2 = ddx * ddx + ddy * ddy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    if (best == static_cast<std::size_t>(-1)) {
        return false;
    }
    m_held_index = best;
    m_held_target_x = wx;
    m_held_target_y = wy;
    return true;
}

void scene::drag_held_node(double wx, double wy) noexcept {
    m_held_target_x = wx;
    m_held_target_y = wy;
}

void scene::release_held_node() noexcept {
    if (m_held_index < m_body_ids.size()) {
        // Re-sync verlet prev_* to current position so we don't inherit a
        // pseudo-velocity from the cursor motion.
        const engine_demo::physics::body* b = m_solver.try_get_body(m_body_ids[m_held_index]);
        if (b != nullptr) {
            m_verlet[m_held_index].prev_x = b->position_x;
            m_verlet[m_held_index].prev_y = b->position_y;
        }
    }
    m_held_index = static_cast<std::size_t>(-1);
}

void scene::spawn_particle_burst(double wx, double wy, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        particle p{};
        p.x = wx;
        p.y = wy;
        const double angle = m_rng.next_double_unit() * 6.2831853;
        const double speed = 1.5 + m_rng.next_double_unit() * 2.5;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.radius = 0.04 + m_rng.next_double_unit() * 0.03;
        m_particles.push_back(p);
        // Grow trail buffer in lockstep, initialized to the spawn point.
        const trail_point t{static_cast<float>(wx), static_cast<float>(wy)};
        for (std::size_t k = 0; k < kTrailLength; ++k) {
            m_trails.push_back(t);
        }
    }
}

void scene::roll_trails() noexcept {
    if (m_particles.empty()) {
        return;
    }
    const std::size_t slot = m_trail_head;
    for (std::size_t i = 0; i < m_particles.size(); ++i) {
        m_trails[i * kTrailLength + slot] =
            trail_point{static_cast<float>(m_particles[i].x), static_cast<float>(m_particles[i].y)};
    }
    m_trail_head = (m_trail_head + 1) % kTrailLength;
}

trail_point scene::trail_at(std::size_t p, std::size_t i) const noexcept {
    // Oldest first: next-write slot is the oldest valid sample.
    const std::size_t slot = (m_trail_head + i) % kTrailLength;
    return m_trails[p * kTrailLength + slot];
}

void scene::substep(double dt) noexcept {
    // Verlet integrate dynamic bodies (anchors keep their rest position).
    for (std::size_t i = 0; i < m_body_ids.size(); ++i) {
        if (m_anchor_flags[i] != 0) {
            continue;
        }
        const std::uint64_t id = m_body_ids[i];
        const engine_demo::physics::body* read = m_solver.try_get_body(id);
        if (read == nullptr) {
            continue;
        }
        const double cur_x = read->position_x;
        const double cur_y = read->position_y;

        const double next_x = cur_x + (cur_x - m_verlet[i].prev_x);
        const double next_y = cur_y + (cur_y - m_verlet[i].prev_y) + (-kGravityY * dt * dt);

        engine_demo::physics::body updated = *read;
        updated.position_x = next_x;
        updated.position_y = next_y;
        m_solver.add_body(updated);  // upsert

        m_verlet[i].prev_x = cur_x;
        m_verlet[i].prev_y = cur_y;
    }

    // Pin the held node to the cursor target before solving so the user's
    // drag wins over physics.
    if (m_held_index < m_body_ids.size()) {
        const std::uint64_t id = m_body_ids[m_held_index];
        const engine_demo::physics::body* read = m_solver.try_get_body(id);
        if (read != nullptr) {
            engine_demo::physics::body updated = *read;
            updated.position_x = m_held_target_x;
            updated.position_y = m_held_target_y;
            m_solver.add_body(updated);
            m_verlet[m_held_index].prev_x = m_held_target_x;
            m_verlet[m_held_index].prev_y = m_held_target_y;
        }
    }

    // Resolve distance constraints.
    (void)m_solver.solve(kSolverIterations);

    // Free particles. Storm scene uses two orbital gravity wells; other
    // scenes use a gentle downward gravity with bouncy world bounds.
    if (m_kind == scene_kind::particle_storm) {
        constexpr double kWellX0 = -1.2;
        constexpr double kWellY0 = 0.4;
        constexpr double kWellX1 = 1.2;
        constexpr double kWellY1 = -0.3;
        constexpr double kPull = 1.4;
        constexpr double kSoft = 0.15;  // softening to bound 1/r^2
        for (std::size_t i = 0; i < m_particles.size(); ++i) {
            particle& p = m_particles[i];
            for (int w = 0; w < 2; ++w) {
                const double wx = (w == 0) ? kWellX0 : kWellX1;
                const double wy = (w == 0) ? kWellY0 : kWellY1;
                const double rx = wx - p.x;
                const double ry = wy - p.y;
                const double r2 = rx * rx + ry * ry + kSoft;
                const double inv_r = 1.0 / std::sqrt(r2);
                const double f = kPull / r2;
                p.vx += rx * inv_r * f * dt;
                p.vy += ry * inv_r * f * dt;
            }
            // Soft drag so orbits don't blow up over long sims.
            p.vx *= 0.999;
            p.vy *= 0.999;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            // Wrap around generous world bounds so storms feel infinite.
            if (p.x < -3.5)
                p.x += 7.0;
            if (p.x > 3.5)
                p.x -= 7.0;
            if (p.y < -2.5)
                p.y += 5.0;
            if (p.y > 2.5)
                p.y -= 5.0;
        }
    } else {
        for (std::size_t i = 0; i < m_particles.size(); ++i) {
            particle& p = m_particles[i];
            p.vy -= kGravityY * dt * 0.25;  // lighter gravity for visual interest
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            if (p.x < -3.0) {
                p.x = -3.0;
                p.vx = -p.vx * 0.85;
            } else if (p.x > 3.0) {
                p.x = 3.0;
                p.vx = -p.vx * 0.85;
            }
            if (p.y < -2.0) {
                p.y = -2.0;
                p.vy = -p.vy * 0.85;
            } else if (p.y > 2.0) {
                p.y = 2.0;
                p.vy = -p.vy * 0.85;
            }
        }
    }

    m_sim_time += dt;
}

std::uint32_t scene::step(double delta_seconds) noexcept {
    m_wall_time += delta_seconds;
    const std::uint32_t substeps = m_loop.advance(delta_seconds);
    for (std::uint32_t i = 0; i < substeps; ++i) {
        substep(kFixedStepSeconds);
        ++m_total_substeps;
    }
    // Trails roll once per frame (render artifact only — never feeds digest).
    if (substeps > 0) {
        roll_trails();
    }
    // Record this whole frame's wall time in milliseconds for the budget HUD.
    m_budget.record_sample(delta_seconds * 1000.0);
    return substeps;
}

bool scene::rope_node_position(std::size_t index, double& x, double& y) const noexcept {
    if (index >= m_body_ids.size()) {
        return false;
    }
    const engine_demo::physics::body* b = m_solver.try_get_body(m_body_ids[index]);
    if (b == nullptr) {
        return false;
    }
    x = b->position_x;
    y = b->position_y;
    return true;
}

std::uint64_t scene::state_digest() const noexcept {
    std::uint64_t h = 0xcbf29ce484222325ull;  // FNV-1a 64-bit offset basis
    for (std::size_t i = 0; i < m_body_ids.size(); ++i) {
        double x = 0.0;
        double y = 0.0;
        if (!rope_node_position(i, x, y)) {
            continue;
        }
        h = fnv1a_mix(h, x);
        h = fnv1a_mix(h, y);
    }
    for (std::size_t i = 0; i < m_particles.size(); ++i) {
        const particle& p = m_particles[i];
        h = fnv1a_mix(h, p.x);
        h = fnv1a_mix(h, p.y);
    }
    return h;
}

}  // namespace ea_sandbox
