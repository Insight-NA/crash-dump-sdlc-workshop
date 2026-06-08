// Deterministic RNG.

#include "engine_demo/sim/rng.h"

namespace engine_demo::sim {

namespace {

[[nodiscard]] std::uint32_t derive_subseed(std::uint64_t seed) noexcept {
    // XOR-fold: widens the high half into the low half before seeding mt19937.
    const auto high = static_cast<std::uint32_t>(seed >> 32);
    const auto low = static_cast<std::uint32_t>(seed & 0xFFFFFFFFu);
    return high ^ low;
}

}  // namespace

rng::rng(std::uint64_t seed) noexcept
    : m_engine{derive_subseed(seed)} {}

std::uint32_t rng::next_u32() noexcept {
    return static_cast<std::uint32_t>(m_engine());
}

double rng::next_double_unit() noexcept {
    // Constitutional article 5: doubles only in sim paths.
    constexpr double kMaxU32Plus1 = 4294967296.0;
    return static_cast<double>(next_u32()) / kMaxU32Plus1;
}

}  // namespace engine_demo::sim
