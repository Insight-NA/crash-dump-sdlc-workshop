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

    if (m_offset + n > m_capacity) {
        return nullptr;
    }
    m_offset = align_up(m_offset, alignment);
    std::byte* ptr = m_buffer + m_offset;
    m_offset += n;
    return ptr;
}

void* allocator::allocate(std::size_t n, std::size_t alignment, std::size_t /*offset*/) noexcept {
    return allocate(n, alignment);
}

void allocator::deallocate(void* /*p*/, std::size_t /*n*/) noexcept {
    // Bump-arena: deallocate is a no-op. Reclaim only via destruction or reset.
}

}  // namespace engine_demo
