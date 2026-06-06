// GoogleTest for engine_demo::allocator.

#include "engine_demo/allocator.h"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>

namespace {

TEST(allocator, basic_allocation_returns_aligned_pointer) {
    std::array<std::byte, 1024> buffer{};
    engine_demo::allocator a{buffer.data(), buffer.size()};

    void* p = a.allocate(64, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 16, 0u);
    EXPECT_GE(a.bytes_used(), 64u);
}

TEST(allocator, exhaustion_returns_null) {
    std::array<std::byte, 64> buffer{};
    engine_demo::allocator a{buffer.data(), buffer.size()};

    void* p1 = a.allocate(48, 8);
    void* p2 = a.allocate(48, 8);
    EXPECT_NE(p1, nullptr);
    EXPECT_EQ(p2, nullptr);
}

// REGRESSION TEST for BUG-001 — re-enabled by crash-qa after Branch B fix.
TEST(allocator, third_aligned_alloc_does_not_overrun_arena) {
    std::array<std::byte, 160> buffer{};
    engine_demo::allocator a{buffer.data(), buffer.size()};

    EXPECT_NE(a.allocate(40, 32), nullptr);
    EXPECT_NE(a.allocate(40, 32), nullptr);
    EXPECT_EQ(a.allocate(40, 32), nullptr) << "third aligned alloc must reject, not overrun";
    EXPECT_LE(a.bytes_used(), buffer.size());
}

}  // namespace
