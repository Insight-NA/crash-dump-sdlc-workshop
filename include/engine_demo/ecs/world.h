// Minimal ECS world with packed entity slots.
//
// Constitutional articles satisfied:
//   - 1, 2, 3, 4 (no exceptions, no RTTI, EASTL, allocator-aware)
//
// SEEDED DEFECT BUG-003: see src/engine_demo/ecs/world.cpp.

#pragma once

#include <EASTL/vector.h>

#include "engine_demo/allocator.h"

#include <cstddef>
#include <cstdint>

namespace engine_demo::ecs {

struct entity_handle {
    std::uint32_t index{0};
    std::uint32_t generation{0};

    [[nodiscard]] bool is_valid() const noexcept { return generation != 0; }
};

[[nodiscard]] inline bool operator==(entity_handle a, entity_handle b) noexcept {
    return a.index == b.index && a.generation == b.generation;
}

class [[nodiscard]] world {
   public:
    explicit world(allocator& alloc) noexcept;

    world(const world&) = delete;
    world& operator=(const world&) = delete;
    world(world&&) noexcept = default;
    world& operator=(world&&) noexcept = default;

    [[nodiscard]] entity_handle create_entity() noexcept;
    void destroy_entity(entity_handle h) noexcept;

    [[nodiscard]] bool is_alive(entity_handle h) const noexcept;

    [[nodiscard]] std::size_t live_count() const noexcept { return m_live_count; }

   private:
    struct slot {
        std::uint32_t generation{0};
        bool alive{false};
    };

    [[maybe_unused]] allocator* m_alloc;
    static constexpr std::size_t kMaxEntities = 4096;
    slot m_slots[kMaxEntities]{};
    std::size_t m_next_free{0};
    std::size_t m_live_count{0};
};

}  // namespace engine_demo::ecs
