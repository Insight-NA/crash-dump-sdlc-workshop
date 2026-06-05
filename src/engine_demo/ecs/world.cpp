// ECS world. Slot-based, generational handles ... ALMOST.
//
// SEEDED DEFECT BUG-003 (Session 04 security anchor, CWE-416 use-after-free):
//   destroy_entity() flips slot.alive to false but does NOT bump slot.generation. The next
//   create_entity() that picks the same slot reuses the prior generation, so any handle
//   captured before the destroy still passes is_alive() against the freshly-recycled slot.
//   Downstream component lookups dereference the wrong logical entity.
//
// The "fix" is one line: `++m_slots[h.index].generation;` in destroy_entity().

#include "engine_demo/ecs/world.h"

namespace engine_demo::ecs {

world::world(allocator& alloc) noexcept : m_alloc{&alloc} {}

entity_handle world::create_entity() noexcept {
    // Linear scan for the first free slot from m_next_free.
    for (std::size_t i = 0; i < kMaxEntities; ++i) {
        const std::size_t idx = (m_next_free + i) % kMaxEntities;
        if (!m_slots[idx].alive) {
            m_slots[idx].alive = true;
            if (m_slots[idx].generation == 0) {
                m_slots[idx].generation = 1;
            }
            m_next_free = (idx + 1) % kMaxEntities;
            ++m_live_count;
            return entity_handle{static_cast<std::uint32_t>(idx), m_slots[idx].generation};
        }
    }
    return entity_handle{};  // pool exhausted
}

void world::destroy_entity(entity_handle h) noexcept {
    if (h.index >= kMaxEntities)
        return;
    auto& s = m_slots[h.index];
    if (!s.alive || s.generation != h.generation)
        return;
    // BUG-003: missing `++s.generation` here. Stale handles will revive on the next
    // create_entity() that recycles this slot.
    s.alive = false;
    if (m_live_count > 0)
        --m_live_count;
}

bool world::is_alive(entity_handle h) const noexcept {
    if (h.index >= kMaxEntities)
        return false;
    const auto& s = m_slots[h.index];
    return s.alive && s.generation == h.generation && h.generation != 0;
}

}  // namespace engine_demo::ecs
