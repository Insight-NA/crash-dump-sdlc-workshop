// Scene — multi-mode physics showcase driven by engine_demo subsystems.
//
// Available scenes (selectable at runtime, see scene_kind):
//   - rope             : single 24-node verlet chain (the original demo)
//   - pendulum_tower   : 4 chains of varying length, anchored across the top
//   - cloth            : 12 x 8 grid with structural + shear constraints
//   - particle_storm   : 500 free particles with two orbital gravity wells
//
// Owns:
//   - allocator (arena) for all EASTL containers in this translation unit
//   - constraint_solver (engine_demo::physics) - exercises BUG-004
//   - game_loop          (engine_demo::sim)    - exercises BUG-002
//   - frame_budget       (engine_demo)         - exercises BUG-006
//   - rng                (engine_demo::sim)    - deterministic seeding
//
// Constitutional articles: 1, 2, 3, 4, 5.

#pragma once

#include <EASTL/vector.h>

#include "engine_demo/allocator.h"
#include "engine_demo/frame_budget.h"
#include "engine_demo/physics/constraint.h"
#include "engine_demo/sim/game_loop.h"
#include "engine_demo/sim/rng.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace ea_sandbox {

enum class scene_kind : std::uint8_t {
    rope = 0,
    pendulum_tower = 1,
    cloth = 2,
    particle_storm = 3,
};

[[nodiscard]] const char* scene_kind_name(scene_kind k) noexcept;

// Storage for verlet integration: previous-frame positions per body id,
// indexed by index into m_body_ids.
struct verlet_state {
    double prev_x{0.0};
    double prev_y{0.0};
};

struct particle {
    double x{0.0};
    double y{0.0};
    double vx{0.0};
    double vy{0.0};
    double radius{0.05};
};

// One historical position per particle, used for motion trails. Float
// precision is intentional: trails are a render-only artifact and never
// feed state_digest, so the determinism digest stays stable.
struct trail_point {
    float x{0.0F};
    float y{0.0F};
};

class [[nodiscard]] scene {
   public:
    static constexpr std::size_t kArenaBytes = 4u << 20;  // 4 MiB
    static constexpr std::size_t kRopeNodes = 24;
    static constexpr std::size_t kPendulumChains = 4;
    static constexpr std::size_t kClothCols = 12;
    static constexpr std::size_t kClothRows = 8;
    static constexpr std::size_t kStormParticles = 500;
    static constexpr std::size_t kRopeFreeParticles = 32;
    static constexpr std::size_t kTrailLength = 10;
    static constexpr double kFixedStepSeconds = 1.0 / 60.0;
    static constexpr double kGravityY = 9.81;
    static constexpr std::uint32_t kSolverIterations = 8;

    explicit scene(std::uint64_t seed);
    ~scene();

    scene(const scene&) = delete;
    scene& operator=(const scene&) = delete;
    scene(scene&&) = delete;
    scene& operator=(scene&&) = delete;

    // Step the simulation by `delta_seconds` of wall time. Returns the number
    // of fixed-step substeps actually run.
    std::uint32_t step(double delta_seconds) noexcept;

    // Reseed and rebuild scene state in-place (preserves current scene_kind).
    void reseed(std::uint64_t seed) noexcept;

    // Switch to a different scene type (re-uses the current seed).
    void switch_scene(scene_kind kind) noexcept;
    [[nodiscard]] scene_kind kind() const noexcept { return m_kind; }

    // Mouse interaction: while a node is held its position is pinned to the
    // dragged location each substep, then released to physics on let-go.
    [[nodiscard]] bool grab_nearest_rope_node(double wx, double wy, double max_dist) noexcept;
    void drag_held_node(double wx, double wy) noexcept;
    void release_held_node() noexcept;
    [[nodiscard]] bool has_held_node() const noexcept { return m_held_index < m_body_ids.size(); }

    // Spawn a small burst of free particles centered on (wx, wy).
    void spawn_particle_burst(double wx, double wy, std::size_t count) noexcept;

    // Read-only accessors for renderers and the headless trace writer.
    [[nodiscard]] std::size_t rope_node_count() const noexcept { return m_body_ids.size(); }
    [[nodiscard]] bool rope_node_position(std::size_t index, double& x, double& y) const noexcept;

    // For renderers that draw constraints as line segments, expose index pairs.
    [[nodiscard]] std::size_t edge_count() const noexcept { return m_edges.size() / 2; }
    void edge(std::size_t i, std::size_t& a, std::size_t& b) const noexcept {
        a = m_edges[2 * i];
        b = m_edges[2 * i + 1];
    }
    // True if rope-node index `i` is a fixed anchor (drawn highlighted).
    [[nodiscard]] bool is_anchor(std::size_t i) const noexcept;
    // Held-node index (sentinel if none). Renderers use this to highlight.
    [[nodiscard]] std::size_t held_index() const noexcept { return m_held_index; }

    [[nodiscard]] std::size_t particle_count() const noexcept { return m_particles.size(); }
    [[nodiscard]] const particle& particle_at(std::size_t index) const noexcept {
        return m_particles[index];
    }
    [[nodiscard]] particle& particle_mut(std::size_t index) noexcept {
        return m_particles[index];
    }
    // Trail readback: returns the i'th trail point for particle `p`, oldest first.
    [[nodiscard]] trail_point trail_at(std::size_t p, std::size_t i) const noexcept;

    [[nodiscard]] const engine_demo::frame_budget& budget() const noexcept { return m_budget; }
    [[nodiscard]] std::uint64_t seed() const noexcept { return m_seed; }
    [[nodiscard]] double sim_time_seconds() const noexcept { return m_sim_time; }
    [[nodiscard]] double wall_time_seconds() const noexcept { return m_wall_time; }
    [[nodiscard]] std::uint64_t total_substeps() const noexcept { return m_total_substeps; }

    // Total constraint edges currently in the solver (for HUD readout).
    [[nodiscard]] std::size_t constraint_edge_count() const noexcept { return m_edges.size() / 2; }

    // Deterministic 64-bit FNV-1a digest of the current rope+particle state.
    // Used by headless mode + the golden-trace CI assertion.
    [[nodiscard]] std::uint64_t state_digest() const noexcept;

   private:
    void rebuild_scene(std::uint64_t seed) noexcept;
    void substep(double dt) noexcept;
    void roll_trails() noexcept;

    void build_rope() noexcept;
    void build_pendulum_tower() noexcept;
    void build_cloth() noexcept;
    void build_particle_storm() noexcept;

    void reset_solver() noexcept;
    void add_pinned_body(double x, double y) noexcept;
    void add_dynamic_body(double x, double y) noexcept;
    void add_edge(std::size_t a, std::size_t b, double rest) noexcept;

    // Backing storage for arena + EASTL containers. Heap-allocated to avoid
    // blowing the default 1 MiB Windows stack (kArenaBytes == 4 MiB).
    unsigned char* m_arena_buffer{nullptr};
    engine_demo::allocator m_alloc;
    engine_demo::eastl_allocator_ref m_alloc_ref;

    std::uint64_t m_seed{0};
    scene_kind m_kind{scene_kind::rope};

    engine_demo::sim::rng m_rng;
    engine_demo::sim::game_loop m_loop;
    engine_demo::physics::constraint_solver m_solver;
    engine_demo::frame_budget m_budget;

    eastl::vector<std::uint64_t, engine_demo::eastl_allocator_ref> m_body_ids;
    eastl::vector<verlet_state, engine_demo::eastl_allocator_ref> m_verlet;
    eastl::vector<std::uint8_t, engine_demo::eastl_allocator_ref> m_anchor_flags;
    eastl::vector<std::size_t, engine_demo::eastl_allocator_ref> m_edges;  // flat (a,b) pairs
    eastl::vector<particle, engine_demo::eastl_allocator_ref> m_particles;
    eastl::vector<trail_point, engine_demo::eastl_allocator_ref> m_trails;  // P * kTrailLength

    // Held-node drag state. m_held_index >= m_body_ids.size() => none held.
    std::size_t m_held_index{static_cast<std::size_t>(-1)};
    double m_held_target_x{0.0};
    double m_held_target_y{0.0};

    std::size_t m_trail_head{0};

    double m_sim_time{0.0};
    double m_wall_time{0.0};
    std::uint64_t m_total_substeps{0};
};

}  // namespace ea_sandbox
