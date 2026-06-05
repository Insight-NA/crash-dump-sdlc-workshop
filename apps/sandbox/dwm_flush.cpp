// dwm_flush.cpp — isolated Windows DWM compositor sync shim.
//
// DwmFlush() forces the Desktop Window Manager to composite the current
// WGL surface to the screen.  On Parallels (WGL-on-Metal), glfwSwapBuffers
// queues a present to the Metal surface but DWM never composites it without
// an explicit flush.
//
// This file MUST NOT include raylib.h.  windows.h redefines Rectangle,
// LoadImage, DrawTextEx, CloseWindow, ShowCursor, etc.

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <cstdio>

namespace ea_sandbox {

static int s_fast_flush_streak = 0;
static int s_flush_call_count  = 0;

void platform_dwm_flush() noexcept {
    LARGE_INTEGER freq{}, t0{}, t1{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    const HRESULT hr = DwmFlush();

    QueryPerformanceCounter(&t1);
    const double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;

    ++s_flush_call_count;
    if (ms < 2.0) {
        ++s_fast_flush_streak;
    } else {
        s_fast_flush_streak = 0;
    }

    // Log on first call, every 60 frames, at threshold crossings, and on error.
    const bool should_log = (s_flush_call_count == 1)
                         || (s_flush_call_count % 60 == 0)
                         || (s_fast_flush_streak == 10)
                         || FAILED(hr);
    if (should_log) {
        std::fprintf(stderr,
            "WINDIAG:dwmflush  call=%d  dt=%.2fms  hr=0x%08lx  fast_streak=%d\n",
            s_flush_call_count, ms, (unsigned long)hr, s_fast_flush_streak);
        std::fflush(stderr);
    }
}

}  // namespace ea_sandbox

#endif  // _WIN32

