// Implementation of engine_demo::allocator. Bump-pointer arena over a caller-provided buffer.

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

    // Align the absolute address, then convert back to a buffer-relative offset.
    const auto base_addr    = reinterpret_cast<std::uintptr_t>(m_buffer);
    const auto aligned_addr = align_up(base_addr + m_offset, alignment);
    const auto aligned_off  = aligned_addr - base_addr;
    if (aligned_off + n > m_capacity) {
        return nullptr;
    }
    m_offset = aligned_off + n;
    return m_buffer + aligned_off;
}

void* allocator::allocate(std::size_t n, std::size_t alignment, std::size_t /*offset*/) noexcept {
    return allocate(n, alignment);
}

void allocator::deallocate(void* /*p*/, std::size_t /*n*/) noexcept {
    // Bump-arena: deallocate is a no-op. Reclaim only via destruction or reset.
}

}  // namespace engine_demo
