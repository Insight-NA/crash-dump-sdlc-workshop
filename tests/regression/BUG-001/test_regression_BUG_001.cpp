// Regression test for BUG-001: arena allocator alignment-padded OOB write AV.
//
// Root cause: allocator::allocate() performed the bounds check using raw
// m_offset + n BEFORE applying alignment padding.  When alignment pushes the
// actual start address past capacity the pointer is OOB but non-null, leading
// to a write AV on first use.
//
// Fix (commit f66466f, src/engine_demo/allocator.cpp):
//   const auto aligned = align_up(m_offset, alignment);   // applied FIRST
//   if (aligned + n > m_capacity) return nullptr;         // check uses aligned
//
// Crash scenario from BUG-001.dmp / repro.cpp:
//   128-byte arena, allocate(40, 32) x3 without null-checks -> WRITE AV on p3
//
// OOB-trigger scenario (H1 allocator fix target):
//   96-byte arena, allocate(40, 32) x2.
//   Pre-fix:  m_offset+n = 40+40 = 80 <= 96 (check passes)
//             but align_up(40,32) = 64 -> 64+40 = 104 > 96 -> OOB ptr returned
//   Post-fix: check uses 64+40 = 104 > 96 -> nullptr returned correctly

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>

#include "engine_demo/allocator.h"
#include "engine_demo/frame_budget.h"

namespace {

// ---------------------------------------------------------------------------
// H1: allocator OOB bounds check — alignment padding causes past-end ptr
// ---------------------------------------------------------------------------

// 96-byte arena, allocate(40, 32) x2.
// Call 2: pre-fix check passes (40+40=80 <=96) but align_up(40,32)=64,
//         so returned region is [64,104) which exceeds arena => OOB ptr.
// Post-fix: bounds check is 64+40=104 > 96 -> nullptr.
// This test fails on pre-fix allocator.cpp and passes on post-fix.
TEST(BUG_001_Regression, aligned_padding_oob_returns_null_not_oob_ptr) {
    static constexpr std::size_t kArenaSize = 96;
    alignas(32) unsigned char buffer[kArenaSize]{};
    engine_demo::allocator a{buffer, kArenaSize};

    void* p1 = a.allocate(40, 32);
    ASSERT_NE(p1, nullptr) << "First allocate(40,32) must succeed in 96-byte arena";

    // Pre-fix: returns buffer+64 (OOB -- region [64,104) vs arena [0,96)).
    // Post-fix: returns nullptr (bounds check uses align_up before comparing).
    void* p2 = a.allocate(40, 32);
    EXPECT_EQ(p2, nullptr)
        << "Second allocate(40,32) must return nullptr: alignment padding "
           "(align_up(40,32)=64) pushes end to byte 104, exceeding 96-byte arena. "
           "Non-null result proves pre-fix OOB regression is present.";
}

// All returned non-null pointers must satisfy requested alignment.
TEST(BUG_001_Regression, allocate_returned_ptr_satisfies_alignment) {
    static constexpr std::size_t kArenaSize = 512;
    alignas(64) unsigned char buffer[kArenaSize]{};
    engine_demo::allocator a{buffer, kArenaSize};

    for (std::size_t align : {1u, 4u, 8u, 16u, 32u, 64u}) {
        void* p = a.allocate(1, align);
        if (p == nullptr) break;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % align, 0u)
            << "Pointer not aligned to " << align;
    }
}

// All returned non-null pointers must lie strictly within the arena.
TEST(BUG_001_Regression, returned_pointers_lie_within_arena_bounds) {
    static constexpr std::size_t kArenaSize = 512;
    alignas(64) unsigned char buffer[kArenaSize]{};
    engine_demo::allocator a{buffer, kArenaSize};
    const auto lo = reinterpret_cast<std::uintptr_t>(buffer);
    const auto hi = lo + kArenaSize;

    for (int i = 0; i < 20; ++i) {
        void* p = a.allocate(16, 16);
        if (p == nullptr) break;
        auto addr = reinterpret_cast<std::uintptr_t>(p);
        EXPECT_GE(addr, lo) << "Pointer below arena start at i=" << i;
        EXPECT_LT(addr + 16, hi) << "Pointer end above arena end at i=" << i;
    }
}

// Zero-capacity allocator must always return nullptr.
TEST(BUG_001_Regression, zero_capacity_allocator_returns_null) {
    engine_demo::allocator a{nullptr, 0};
    EXPECT_EQ(a.allocate(1, 1), nullptr);
}

// ---------------------------------------------------------------------------
// H2: null-guard pattern -- write-through-null does not occur
// ---------------------------------------------------------------------------

// Reproduces the caller-side pattern from repro.cpp / capture_self.cpp.
// 128-byte arena: third allocate(40,32) correctly returns nullptr (arena full).
// The H2 fix adds null-checks so the caller does not write through null.
TEST(BUG_001_Regression, exhausted_arena_returns_null_and_caller_guards) {
    static constexpr std::size_t kArenaSize = 128;
    alignas(32) unsigned char buffer[kArenaSize]{};
    engine_demo::allocator a{buffer, kArenaSize};

    void* p1 = a.allocate(40, 32);
    ASSERT_NE(p1, nullptr);
    if (p1 != nullptr) { static_cast<unsigned char*>(p1)[0] = 0xAA; }

    void* p2 = a.allocate(40, 32);
    ASSERT_NE(p2, nullptr);
    if (p2 != nullptr) { static_cast<unsigned char*>(p2)[0] = 0xBB; }

    void* p3 = a.allocate(40, 32);
    // p3 must be null (arena at byte 104, need align(104,32)=128+40=168>128).
    EXPECT_EQ(p3, nullptr) << "Third allocate must return nullptr on exhausted 128-byte arena";
    // Guarded caller pattern: never write through p3 if null.
    if (p3 != nullptr) { static_cast<unsigned char*>(p3)[0] = 0xCC; }
    SUCCEED() << "No write-through-null AV observed";
}

// ---------------------------------------------------------------------------
// H3: frame_budget m_alloc{nullptr} default init
// ---------------------------------------------------------------------------

// frame_budget constructed with valid allocator must return it via get_allocator().
// H3 fix adds {nullptr} default init to m_alloc preventing indeterminate state.
TEST(BUG_001_Regression, frame_budget_get_allocator_identity) {
    static constexpr std::size_t kArenaSize = 4096;
    alignas(16) unsigned char buf[kArenaSize]{};
    engine_demo::allocator alloc{buf, kArenaSize};

    engine_demo::frame_budget fb{alloc, 10};
    const engine_demo::allocator& returned = fb.get_allocator();
    EXPECT_EQ(&returned, &alloc)
        << "get_allocator() must return the allocator passed to the constructor";
}

}  // namespace
