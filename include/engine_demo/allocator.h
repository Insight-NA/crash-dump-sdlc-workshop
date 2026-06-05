// engine_demo public allocator interface.
//
// Constitutional articles satisfied:
//   - 1 (no exceptions)
//   - 2 (no RTTI)
//   - 4 (allocator-aware: this IS the allocator)
//
// Seeded defect: BUG-001 (OOB write inside arena bookkeeping). See fixtures/seeded-bugs.md.

#pragma once

#include <cstddef>
#include <cstdint>

namespace engine_demo {

// Status return for fallible allocator operations.
enum class alloc_status : std::uint8_t {
    ok = 0,
    out_of_memory,
    invalid_alignment,
    invalid_size,
};

// EASTL-compatible allocator interface. Construct over a contiguous buffer;
// calls do not throw. All sizes are in bytes.
class [[nodiscard]] allocator {
   public:
    allocator(void* buffer, std::size_t capacity_bytes) noexcept;

    allocator(const allocator&) = delete;
    allocator& operator=(const allocator&) = delete;
    allocator(allocator&&) noexcept;
    allocator& operator=(allocator&&) noexcept;
    ~allocator();

    // Allocate `n` bytes aligned to `alignment`. Returns nullptr on failure.
    [[nodiscard]] void* allocate(std::size_t n,
                                 std::size_t alignment = alignof(std::max_align_t)) noexcept;

    // EASTL allocator hook (overload that takes an offset alignment).
    [[nodiscard]] void* allocate(std::size_t n, std::size_t alignment, std::size_t offset) noexcept;

    void deallocate(void* p, std::size_t n) noexcept;

    [[nodiscard]] const char* get_name() const noexcept { return m_name; }
    void set_name(const char* name) noexcept { m_name = name; }

    [[nodiscard]] std::size_t bytes_used() const noexcept { return m_offset; }
    [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }

   private:
    std::byte* m_buffer{nullptr};
    std::size_t m_capacity{0};
    std::size_t m_offset{0};
    const char* m_name{"engine_demo::allocator"};
};

[[nodiscard]] inline bool operator==(const allocator& a, const allocator& b) noexcept {
    return &a == &b;
}
[[nodiscard]] inline bool operator!=(const allocator& a, const allocator& b) noexcept {
    return &a != &b;
}

// EASTL-allocator-shaped reference adapter. Holds a non-owning pointer to an
// engine_demo::allocator and forwards EASTL's allocator concept calls. Use this
// type as the allocator template parameter for every EASTL container in
// committed code, satisfying constitutional Article 4 (allocator-aware).
//
// Construction is explicit; copying is allowed and shares the same backing
// allocator. Move semantics are trivial.
class [[nodiscard]] eastl_allocator_ref {
   public:
    eastl_allocator_ref() noexcept = default;  // EASTL requires default-constructibility
    explicit eastl_allocator_ref(allocator& alloc) noexcept : m_alloc{&alloc} {}
    eastl_allocator_ref(const eastl_allocator_ref&) noexcept = default;
    eastl_allocator_ref& operator=(const eastl_allocator_ref&) noexcept = default;
    eastl_allocator_ref(eastl_allocator_ref&&) noexcept = default;
    eastl_allocator_ref& operator=(eastl_allocator_ref&&) noexcept = default;

    [[nodiscard]] void* allocate(std::size_t n, int /*flags*/ = 0) noexcept {
        return m_alloc != nullptr ? m_alloc->allocate(n) : nullptr;
    }
    [[nodiscard]] void* allocate(std::size_t n,
                                 std::size_t alignment,
                                 std::size_t offset,
                                 int /*flags*/ = 0) noexcept {
        return m_alloc != nullptr ? m_alloc->allocate(n, alignment, offset) : nullptr;
    }
    void deallocate(void* p, std::size_t n) noexcept {
        if (m_alloc != nullptr)
            m_alloc->deallocate(p, n);
    }

    [[nodiscard]] const char* get_name() const noexcept {
        return m_alloc != nullptr ? m_alloc->get_name() : "engine_demo::eastl_allocator_ref";
    }
    void set_name(const char* name) noexcept {
        if (m_alloc != nullptr)
            m_alloc->set_name(name);
    }

    [[nodiscard]] allocator* underlying() const noexcept { return m_alloc; }

   private:
    allocator* m_alloc{nullptr};
};

[[nodiscard]] inline bool operator==(const eastl_allocator_ref& a,
                                     const eastl_allocator_ref& b) noexcept {
    return a.underlying() == b.underlying();
}
[[nodiscard]] inline bool operator!=(const eastl_allocator_ref& a,
                                     const eastl_allocator_ref& b) noexcept {
    return a.underlying() != b.underlying();
}

}  // namespace engine_demo
