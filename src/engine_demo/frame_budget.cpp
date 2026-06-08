// frame_budget — rolling window timings.

#include "engine_demo/frame_budget.h"

namespace engine_demo {

frame_budget::frame_budget(allocator& alloc, std::size_t window_size) noexcept
    : m_alloc{&alloc}, m_window_size{window_size > 256 ? 256 : window_size} {}

void frame_budget::record_sample(double milliseconds) noexcept {
    m_samples[m_index] = milliseconds;
    // BUG-006: increment is post-write; the "fix" is to either pre-increment OR ensure
    // m_count tracks the *minimum* of (writes_so_far, window_size) and the read iterates
    // from (m_index - m_count) wrapping. The current code over-counts on warm-up.
    m_index = (m_index + 1) % m_window_size;
    if (m_count < m_window_size) {
        ++m_count;
    }
}

double frame_budget::rolling_average() const noexcept {
    if (m_count == 0)
        return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < m_count; ++i) {
        const std::size_t idx = (m_index + m_window_size - m_count + i) % m_window_size;
        sum += m_samples[idx];
    }
    return sum / static_cast<double>(m_count);
}

}  // namespace engine_demo
