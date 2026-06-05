// Self-capturing reproducer for BUG-001.
// Wraps the same crash scenario as repro.cpp but uses MiniDumpWriteDump via an
// SEH filter so no external dump tools are needed.
//
// Build via capture_self.ps1 (generated alongside this file).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include "engine_demo/allocator.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

static std::byte* alloc_with_guard(std::size_t usable_size) noexcept {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const std::size_t page = si.dwPageSize;
    void* base = VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) return nullptr;
    DWORD old_protect{};
    VirtualProtect(static_cast<std::byte*>(base) + page, page, PAGE_NOACCESS, &old_protect);
    return static_cast<std::byte*>(base) + page - usable_size;
}

static LONG WINAPI dump_and_exit(EXCEPTION_POINTERS* ep) {
    HANDLE hFile = CreateFileA("BUG-001.dmp",
                               GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "ERROR: cannot create BUG-001.dmp (GLE=%lu)\n", GetLastError());
        return EXCEPTION_CONTINUE_SEARCH;
    }
    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId          = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers    = FALSE;
    BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                hFile, MiniDumpWithFullMemory, &mei, nullptr, nullptr);
    CloseHandle(hFile);
    if (ok) {
        printf("BUG-001.dmp written successfully.\n");
    } else {
        fprintf(stderr, "MiniDumpWriteDump failed GLE=%lu\n", GetLastError());
    }
    return EXCEPTION_EXECUTE_HANDLER;  // terminate gracefully after dump
}

int main() {
    SetUnhandledExceptionFilter(dump_and_exit);

    constexpr std::size_t arena_size = 160;
    std::byte* buffer = alloc_with_guard(arena_size);
    if (!buffer) {
        printf("Failed to allocate guarded arena.\n");
        return 1;
    }
    std::memset(buffer, 0, arena_size);

    engine_demo::allocator a{buffer, arena_size};

    void* p1 = a.allocate(40, 32);
    void* p2 = a.allocate(40, 32);
    void* p3 = a.allocate(40, 32);

    printf("p1=%p p2=%p p3=%p used=%zu capacity=%zu\n",
           p1, p2, p3, a.bytes_used(), a.capacity());

    if (p3 != nullptr) {
        printf("BUG: allocator returned p3 inside overrun region -- writing...\n");
        for (std::size_t i = 0; i < 40; ++i) {
            static_cast<std::byte*>(p3)[i] = static_cast<std::byte>(i);
        }
    }

    printf("No crash triggered (check arena sizing or bug state).\n");
    return 0;
}
