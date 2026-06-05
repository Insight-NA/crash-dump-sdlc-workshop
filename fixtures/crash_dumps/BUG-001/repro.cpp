// Minimal reproducer for BUG-001 (allocator OOB write).
//
// Build: $ c++ -std=c++20 -fno-exceptions -fno-rtti -O0 -g -I../../../include \
//             -I../../../src ../../../src/engine_demo/allocator.cpp repro.cpp -o repro
// Run:   $ ./repro     (expect SIGSEGV / access violation crash)
//
// The arena buffer is placed immediately before a guard page so any OOB write
// triggers an instant fault — producing a clean crash dump for the demo.

#include "engine_demo/allocator.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Allocate `usable_size` bytes backed by a guard page immediately after.
// The arena is placed at the *end* of the last usable page so the guard
// page starts at exactly byte `usable_size` — any OOB write is an instant fault.
static std::byte* alloc_with_guard(std::size_t usable_size) noexcept {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const std::size_t page = si.dwPageSize;
    // One usable page + one guard page.
    void* base = VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) return nullptr;
    // Guard the second page.
    DWORD old_protect{};
    VirtualProtect(static_cast<std::byte*>(base) + page, page, PAGE_NOACCESS, &old_protect);
    // Return a pointer ending exactly at the page boundary.
    return static_cast<std::byte*>(base) + page - usable_size;
#else
    const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* base = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return nullptr;
    mprotect(static_cast<std::byte*>(base) + page, page, PROT_NONE);
    return static_cast<std::byte*>(base) + page - usable_size;
#endif
}

int main() {
    constexpr std::size_t arena_size = 160;
    std::byte* buffer = alloc_with_guard(arena_size);
    if (!buffer) {
        std::printf("Failed to allocate guarded arena.\n");
        return 1;
    }
    std::memset(buffer, 0, arena_size);

    engine_demo::allocator a{buffer, arena_size};

    void* p1 = a.allocate(40, 32);
    void* p2 = a.allocate(40, 32);
    void* p3 = a.allocate(40, 32);

    std::printf("p1=%p p2=%p p3=%p used=%zu capacity=%zu\n",
                p1, p2, p3, a.bytes_used(), a.capacity());

    if (p3 != nullptr) {
        std::printf("BUG: allocator returned p3 inside overrun region — writing...\n");
        // This write crosses the guard page boundary -> instant crash.
        for (std::size_t i = 0; i < 40; ++i) {
            static_cast<std::byte*>(p3)[i] = static_cast<std::byte>(i);
        }
    }

    std::printf("No crash (bug not triggered). Check arena sizing.\n");
    return 0;
}
