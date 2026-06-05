// Deterministic RNG.
//
// SEEDED DEFECT BUG-005 (Session 04 security anchor, CWE-197):
//   The 64-bit seed is forwarded via static_cast<uint32_t>, silently dropping the high 32
//   bits. Two distinct uint64_t seeds that share their low 32 bits produce identical
//   streams — a determinism *and* a security regression for any system that derives
//   per-session entropy from a wider source.
//
// FALSE-POSITIVE companion FP-001: the seed-derivation in `derive_subseed` below appears
// to truncate but in fact widens the result via XOR-fold. Learners must dismiss it during
// Session 04's triage rubric.

#include "engine_demo/sim/rng.h"

namespace engine_demo::sim {

namespace {

[[nodiscard, maybe_unused]] std::uint32_t derive_subseed(std::uint64_t seed) noexcept {
    // FP-001 (false positive): looks like truncation; actually XOR-folds the high half
    // into the low half before passing through. NOT the bug.
    const auto high = static_cast<std::uint32_t>(seed >> 32);
    const auto low = static_cast<std::uint32_t>(seed & 0xFFFFFFFFu);
    return high ^ low;
}

}  // namespace

rng::rng(std::uint64_t seed) noexcept
    // BUG-005: this static_cast drops the upper 32 bits of seed entirely.
    // The "fix" is
    // m_engine{std::mt19937{static_cast<std::mt19937::result_type>(derive_subseed(seed))}} —
    // i.e. fold the 64-bit width into the engine's seed type via derive_subseed, not a naked
    // downcast.
    : m_engine{static_cast<std::mt19937::result_type>(seed)} {}

std::uint32_t rng::next_u32() noexcept {
    return static_cast<std::uint32_t>(m_engine());
}

double rng::next_double_unit() noexcept {
    // Constitutional article 5: doubles only in sim paths.
    constexpr double kMaxU32Plus1 = 4294967296.0;
    return static_cast<double>(next_u32()) / kMaxU32Plus1;
}

}  // namespace engine_demo::sim
