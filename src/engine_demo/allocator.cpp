// Implementation of engine_demo::allocator. Bump-pointer arena over a caller-provided buffer.
//
// FIX BUG-001 (Session 02): bounds check now uses the post-alignment offset so that
// alignment padding is correctly accounted for before approving the allocation.

#include "engine_demo/allocator.h"

#include <cstring>

namespace engine_demo {

namespace {

[[nodiscard]] std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    // alignment is required to be a power of two by callers.
    return (value + (alignment - 1)) & ~(alignment - 1);
}

}  // namespace

allocator::allocator(void* buffer, std::size_t capacity_bytes) noexcept
    : m_buffer{static_cast<std::byte*>(buffer)}, m_capacity{capacity_bytes}, m_offset{0} {}

allocator::allocator(allocator&& other) noexcept
    : m_buffer{other.m_buffer},
      m_capacity{other.m_capacity},
      m_offset{other.m_offset},
      m_name{other.m_name} {
    other.m_buffer = nullptr;
    other.m_capacity = 0;
    other.m_offset = 0;
}

allocator& allocator::operator=(allocator&& other) noexcept {
    if (this != &other) {
        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        m_offset = other.m_offset;
        m_name = other.m_name;
        other.m_buffer = nullptr;
        other.m_capacity = 0;
        other.m_offset = 0;
    }
    return *this;
}

allocator::~allocator() = default;

void* allocator::allocate(std::size_t n, std::size_t alignment) noexcept {
    if (n == 0 || alignment == 0)
        return nullptr;
    if ((alignment & (alignment - 1)) != 0)
        return nullptr;  // not a power of two

    // Align the *absolute* pointer address, not just the offset within the buffer.
    // Using the offset alone only works when m_buffer itself is maximally aligned.
    const auto base = reinterpret_cast<std::uintptr_t>(m_buffer);
    const std::size_t aligned_start =
        static_cast<std::size_t>(align_up(base + m_offset, alignment) - base);

    if (aligned_start + n > m_capacity) {
        return nullptr;
    }
    std::byte* aligned_pointer = m_buffer + aligned_start;
    m_offset = aligned_start + n;
    return aligned_pointer;
}

void* allocator::allocate(std::size_t n, std::size_t alignment, std::size_t /*offset*/) noexcept {
    return allocate(n, alignment);
}

void allocator::deallocate(void* /*p*/, std::size_t /*n*/) noexcept {
    // Bump-arena: deallocate is a no-op. Reclaim only via destruction or reset.
}

}  // namespace engine_demo
