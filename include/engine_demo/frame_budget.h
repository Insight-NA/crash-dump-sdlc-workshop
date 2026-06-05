// Frame budget — rolling window of frame timings.
//
// Constitutional articles satisfied:
//   - 5 (determinism: doubles only)
//   - 6 (real-time: no allocation in record_sample)
//
// SEEDED DEFECT BUG-006: see src/engine_demo/frame_budget.cpp.

#pragma once

#include "engine_demo/allocator.h"

#include <cstddef>

namespace engine_demo {

class [[nodiscard]] frame_budget {
   public:
    explicit frame_budget(allocator& alloc, std::size_t window_size = 64) noexcept;

    frame_budget(const frame_budget&) = delete;
    frame_budget& operator=(const frame_budget&) = delete;
    frame_budget(frame_budget&&) noexcept = default;
    frame_budget& operator=(frame_budget&&) noexcept = default;

    void record_sample(double milliseconds) noexcept;

    [[nodiscard]] double rolling_average() const noexcept;
    [[nodiscard]] std::size_t sample_count() const noexcept { return m_count; }
    [[nodiscard]] const allocator& get_allocator() const noexcept { return *m_alloc; }

   private:
    allocator* m_alloc;
    std::size_t m_window_size;
    std::size_t m_index{0};
    std::size_t m_count{0};
    double m_samples[256]{};  // window_size <= 256 by construction; fixed-size avoids allocation in
                              // record_sample (Article 6)
};

}  // namespace engine_demo
