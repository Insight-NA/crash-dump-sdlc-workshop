// EASTL global operator new[] overrides.
//
// EASTL requires the host application to provide these two operator new[] overloads;
// without them link-time references from EASTL containers fail. We forward to malloc
// and intentionally ignore the debug-tracking parameters — the engine_demo workspace
// uses engine_demo::eastl_allocator_ref for any container that should live on a
// caller-supplied arena. These globals exist only to keep EASTL's internal default
// path (and any non-allocator-template-parameter call sites) buildable.

#include <cstdlib>
#include <new>

void* operator new[](std::size_t size,
                     const char* /*name*/,
                     int /*flags*/,
                     unsigned /*debug_flags*/,
                     const char* /*file*/,
                     int /*line*/) noexcept {
    return std::malloc(size);
}

void* operator new[](std::size_t size,
                     std::size_t alignment,
                     std::size_t /*alignment_offset*/,
                     const char* /*name*/,
                     int /*flags*/,
                     unsigned /*debug_flags*/,
                     const char* /*file*/,
                     int /*line*/) noexcept {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* p = nullptr;
    if (alignment < sizeof(void*))
        alignment = sizeof(void*);
    if (posix_memalign(&p, alignment, size) != 0)
        return nullptr;
    return p;
#endif
}
