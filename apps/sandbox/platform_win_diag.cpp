  
// platform_win_diag.cpp - Windows GL/DWM/HWND diagnostics + fixes.
// MUST NOT include raylib.h - windows.h redefines Rectangle, LoadImage, etc.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <wingdi.h>
#include <shellscalingapi.h>
// wglGetCurrentContext/wglGetCurrentDC declared in wingdi.h
// glFinish/wglSwapIntervalEXT loaded dynamically from opengl32/wgl
typedef void  (WINAPI *PFN_glFinish)(void);
typedef BOOL  (WINAPI *PFN_wglSwapIntervalEXT)(int);
static PFN_glFinish            s_glFinish            = nullptr;
static PFN_wglSwapIntervalEXT  s_wglSwapIntervalEXT  = nullptr;

static void load_wgl_procs() noexcept {
    if (s_glFinish) return;
    HMODULE gl = GetModuleHandleW(L"opengl32.dll");
    if (!gl) return;
    s_glFinish = reinterpret_cast<PFN_glFinish>(GetProcAddress(gl, "glFinish"));
    // wglSwapIntervalEXT lives in the ICD (opengl32 delegates via wglGetProcAddress)
    typedef PROC (WINAPI *PFN_wglGetProcAddress)(LPCSTR);
    auto wgpa = reinterpret_cast<PFN_wglGetProcAddress>(GetProcAddress(gl, "wglGetProcAddress"));
    if (wgpa)
        s_wglSwapIntervalEXT = reinterpret_cast<PFN_wglSwapIntervalEXT>(
            wgpa("wglSwapIntervalEXT"));
}
#include <cstdio>
#include <vector>

namespace ea_sandbox {

static HWND get_hwnd() noexcept {
    DWORD pid = GetCurrentProcessId();
    struct Ctx { DWORD pid; HWND h; };
    Ctx c{pid, nullptr};
    EnumWindows([](HWND h, LPARAM l) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(l);
        DWORD p = 0;
        GetWindowThreadProcessId(h, &p);
        if (p == c->pid && IsWindow(h)) { c->h = h; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&c));
    return c.h;
}

void platform_win_diag_startup() noexcept {
    std::fprintf(stderr, "\nWINDIAG:=== startup ===\n");
    HWND h = get_hwnd();
    if (!h) { std::fprintf(stderr, "WINDIAG: ERROR no HWND\n"); return; }

    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    RECT wr{}, cr{};
    GetWindowRect(h, &wr); GetClientRect(h, &cr);

    std::fprintf(stderr, "WINDIAG: HWND=%p vis=%d\n", (void*)h, (int)IsWindowVisible(h));
    std::fprintf(stderr, "WINDIAG: GWL_EXSTYLE=0x%llx LAYERED=%d NOREDIRECT=%d\n",
        (unsigned long long)ex, !!(ex & WS_EX_LAYERED), !!(ex & WS_EX_NOREDIRECTIONBITMAP));
    std::fprintf(stderr, "WINDIAG: WindowSize=%ldx%ld ClientSize=%ldx%ld\n",
        wr.right-wr.left, wr.bottom-wr.top, cr.right-cr.left, cr.bottom-cr.top);

    DWORD cl = 0;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cl, sizeof(cl));
    BOOL comp = FALSE;
    DwmIsCompositionEnabled(&comp);
    std::fprintf(stderr, "WINDIAG: cloaked=%lu composition=%d\n", (unsigned long)cl, (int)comp);

    HDC hdc = GetDC(h);
    if (hdc) {
        int pf = GetPixelFormat(hdc);
        PIXELFORMATDESCRIPTOR pfd{}; pfd.nSize = sizeof(pfd);
        DescribePixelFormat(hdc, pf, sizeof(pfd), &pfd);
        std::fprintf(stderr,
            "WINDIAG: PF=%d flags=0x%lx DBUF=%d SWAP_COPY=%d COMP=%d\n",
            pf, (unsigned long)pfd.dwFlags,
            !!(pfd.dwFlags & PFD_DOUBLEBUFFER),
            !!(pfd.dwFlags & PFD_SWAP_COPY),
            !!(pfd.dwFlags & PFD_SUPPORT_COMPOSITION));
        if (pfd.dwFlags & PFD_SUPPORT_COMPOSITION)
            std::fprintf(stderr,
                "WINDIAG: WARNING PFD_SUPPORT_COMPOSITION - DWM redirects GL to offscreen.\n"
                "WINDIAG:   Parallels Metal never fills it. NOREDIRECTIONBITMAP fix applied.\n");
        HGLRC g = wglGetCurrentContext();
        HDC   d = wglGetCurrentDC();
        std::fprintf(stderr, "WINDIAG: wgl=%p dc_match=%d\n", (void*)g, (d == hdc));
        ReleaseDC(h, hdc);
    } else {
        std::fprintf(stderr, "WINDIAG: ERROR GetDC null\n");
    }

    UINT dpi = GetDpiForWindow(h);
    std::fprintf(stderr, "WINDIAG: DPI=%u scale=%.2fx\n", dpi, (double)dpi / 96.0);

    HMODULE sc = LoadLibraryW(L"shcore.dll");
    if (sc) {
        typedef HRESULT (WINAPI *F)(HANDLE, PROCESS_DPI_AWARENESS*);
        auto f = reinterpret_cast<F>(GetProcAddress(sc, "GetProcessDpiAwareness"));
        if (f) {
            PROCESS_DPI_AWARENESS a = PROCESS_DPI_UNAWARE;
            f(nullptr, &a);
            const char* names[] = {"UNAWARE", "SYSTEM", "PER_MONITOR"};
            std::fprintf(stderr, "WINDIAG: DpiAwareness=%s\n", a <= 2 ? names[a] : "?");
        }
        FreeLibrary(sc);
    }

    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (u) {
        typedef DPI_AWARENESS_CONTEXT (WINAPI *G)();
        typedef BOOL (WINAPI *E)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
        auto g = reinterpret_cast<G>(GetProcAddress(u, "GetThreadDpiAwarenessContext"));
        auto e = reinterpret_cast<E>(GetProcAddress(u, "AreDpiAwarenessContextsEqual"));
        if (g && e) {
            auto c2 = g();
            const char* n =
                e(c2, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ? "PMv2" :
                e(c2, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)    ? "PM"   :
                e(c2, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)         ? "SYS"  : "UNK";
            std::fprintf(stderr, "WINDIAG: ThreadDpiCtx=%s\n", n);
        }
    }

    std::fprintf(stderr, "WINDIAG:=== end startup ===\n\n");
    std::fflush(stderr);
}

void platform_win_diag_frame(int frame) noexcept {
    if (frame % 60 != 0) return;
    HWND h = get_hwnd();
    if (!h) return;
    DWORD cl = 0;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cl, sizeof(cl));
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    // Query current WGL swap interval to confirm it stayed at 0
    typedef int (WINAPI *PFN_wglGetSwapIntervalEXT)(void);
    int swap_interval = -1;
    {
        HMODULE gl = GetModuleHandleW(L"opengl32.dll");
        if (gl) {
            typedef PROC (WINAPI *PFN_wglGetProcAddress)(LPCSTR);
            auto wgpa = reinterpret_cast<PFN_wglGetProcAddress>(GetProcAddress(gl, "wglGetProcAddress"));
            if (wgpa) {
                auto fn = reinterpret_cast<PFN_wglGetSwapIntervalEXT>(wgpa("wglGetSwapIntervalEXT"));
                if (fn) swap_interval = fn();
            }
        }
    }
    std::fprintf(stderr,
        "WINDIAG:frame f=%d vis=%d cloaked=%lu noredirect=%d wgl=%p swap_interval=%d\n",
        frame, (int)IsWindowVisible(h), (unsigned long)cl,
        !!(ex & WS_EX_NOREDIRECTIONBITMAP),
        (void*)wglGetCurrentContext(), swap_interval);
    std::fflush(stderr);
}

void platform_win_gl_finish() noexcept {
    load_wgl_procs();
    if (s_glFinish) s_glFinish();
}

void platform_win_force_present() noexcept {
    HWND h = get_hwnd();
    if (!h) return;
    InvalidateRect(h, nullptr, FALSE);
    UpdateWindow(h);
}

// ---- CBT hook: inject WS_EX_NOREDIRECTIONBITMAP at CreateWindowEx time -----
// SetWindowLongPtr(GWL_EXSTYLE) after window creation is silently ignored by
// DWM for this style flag. The only reliable path is to have the style already
// set when NtUserCreateWindowEx is called. A thread-local WH_CBT hook fires
// inside CreateWindowEx *before* the window is shown, giving us a chance to
// patch the extended style via the CREATESTRUCT pointer on HCBT_CREATEWND.

static HHOOK  s_cbt_hook = nullptr;

static LRESULT CALLBACK cbt_proc(int nCode, WPARAM wParam, LPARAM lParam) noexcept {
    if (nCode == HCBT_CREATEWND) {
        auto* cs = reinterpret_cast<CBT_CREATEWNDW*>(lParam);
        if (cs && cs->lpcs) {
            // wParam = HWND being created; inject WS_EX_NOREDIRECTIONBITMAP now.
            // NOTE: GLFW may call SetWindowLongPtr(GWL_EXSTYLE) after creation
            // and strip this bit.  We log both HWND and parent to cross-reference
            // with get_hwnd() in diag_startup().
            cs->lpcs->dwExStyle |= WS_EX_NOREDIRECTIONBITMAP;
            std::fprintf(stderr,
                "WINDIAG: CBT HCBT_CREATEWND hwnd=%p parent=%p injected WS_EX_NOREDIRECTIONBITMAP.\n",
                (void*)wParam, (void*)cs->lpcs->hwndParent);
            std::fflush(stderr);
        }
    }
    return CallNextHookEx(s_cbt_hook, nCode, wParam, lParam);
}

void platform_win_pre_init() noexcept {
    if (s_cbt_hook) return;
    s_cbt_hook = SetWindowsHookExW(WH_CBT, cbt_proc, nullptr, GetCurrentThreadId());
    if (s_cbt_hook)
        std::fprintf(stderr, "WINDIAG: CBT hook installed (WH_CBT on tid=%lu).\n",
            (unsigned long)GetCurrentThreadId());
    else
        std::fprintf(stderr, "WINDIAG: WARNING SetWindowsHookEx failed err=%lu.\n",
            (unsigned long)GetLastError());
    std::fflush(stderr);
}

void platform_win_post_init() noexcept {
    // Safety: unhook if InitWindow somehow returned without firing HCBT_CREATEWND
    if (s_cbt_hook) {
        UnhookWindowsHookEx(s_cbt_hook);
        s_cbt_hook = nullptr;
        std::fprintf(stderr, "WINDIAG: CBT hook removed in post_init (no HCBT_CREATEWND fired).\n");
        std::fflush(stderr);
    }
}

// ---------------------------------------------------------------------------
// Forward declarations needed by platform_win_setup and platform_win_gdi_present
typedef void (WINAPI *PFN_glReadPixels)(int,int,int,int,unsigned,unsigned,void*);
typedef void (WINAPI *PFN_glGetIntegerv)(unsigned, int*);
static PFN_glReadPixels   s_glReadPixels   = nullptr;
static PFN_glGetIntegerv  s_glGetIntegerv  = nullptr;
static bool               s_use_gdi_present = false;

// ---------------------------------------------------------------------------

void platform_win_setup() noexcept {
    HWND h = get_hwnd();
    if (!h) return;

    load_wgl_procs();

    // Check if this pixel format has PFD_SUPPORT_COMPOSITION.  On Parallels
    // (WGL-on-Metal), this flag means DWM creates a GDI redirection bitmap
    // that the Metal GPU never fills — window stays white/gray.  Enable the
    // GDI blit fallback so glReadPixels+StretchDIBits copies the framebuffer
    // into that surface on every frame.
    {
        HDC hdc = GetDC(h);
        if (hdc) {
            int pf = GetPixelFormat(hdc);
            PIXELFORMATDESCRIPTOR pfd{}; pfd.nSize = sizeof(pfd);
            DescribePixelFormat(hdc, pf, sizeof(pfd), &pfd);
            if (pfd.dwFlags & PFD_SUPPORT_COMPOSITION) {
                s_use_gdi_present = true;
                std::fprintf(stderr,
                    "WINDIAG: PFD_SUPPORT_COMPOSITION detected - enabling GDI blit fallback.\n");
            }
            ReleaseDC(h, hdc);
        }
    }

    // Disable WGL vsync (wglSwapIntervalEXT(0)).
    // FLAG_VSYNC_HINT makes GLFW call wglSwapIntervalEXT(1), but on Parallels
    // (WGL-on-Metal, PFD_SUPPORT_COMPOSITION) this creates a double-vsync:
    // WGL blocks waiting for a vsync AND DwmFlush also blocks for a vsync.
    // The two vsyncs fight each other causing frames to stall and never present
    // visibly. With WGL interval=0, SwapBuffers returns immediately and
    // DwmFlush (called before each swap) owns all vsync timing.
    if (s_wglSwapIntervalEXT) {
        s_wglSwapIntervalEXT(0);
        std::fprintf(stderr, "WINDIAG: Set wglSwapInterval(0) - DwmFlush owns vsync timing.\n");
    } else {
        std::fprintf(stderr, "WINDIAG: WARNING wglSwapIntervalEXT not found.\n");
    }

    // WS_EX_NOREDIRECTIONBITMAP note: must be set at CreateWindowEx time to
    // take effect. Setting post-creation is silently ignored by DWM.
    // Log current state for diagnostics but do not attempt to set it here.
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    std::fprintf(stderr, "WINDIAG: WS_EX_NOREDIRECTIONBITMAP=%d (post-create set is ignored by DWM)\n",
        !!(ex & WS_EX_NOREDIRECTIONBITMAP));

    BOOL no_t = TRUE;
    DwmSetWindowAttribute(h, DWMWA_TRANSITIONS_FORCEDISABLED, &no_t, sizeof(no_t));

    DWORD cl = 0;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cl, sizeof(cl));
    if (cl) { ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); BringWindowToTop(h); }
    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    std::fflush(stderr);
}

// ---- GDI blit fallback -------------------------------------------------
// When PFD_SUPPORT_COMPOSITION is set, DWM creates an offscreen redirection
// bitmap for the window.  On Parallels (Metal-backed WGL), the Metal GL layer
// writes to GPU memory that the GDI-compatible redirection bitmap never sees,
// so the window appears white/gray.  WS_EX_NOREDIRECTIONBITMAP would bypass
// this, but GLFW strips it during post-create style changes.
//
// Workaround: read the GL framebuffer with glReadPixels after every swap, then
// StretchDIBits into the window's DC.  This writes into the GDI surface that
// DWM actually composites, making the rendered scene visible.
//
// Performance: glReadPixels is a GPU stall + CPU copy -- expensive, but
// acceptable for a workshop demo tool.  Disable via s_use_gdi_present = false
// on hardware where the normal swap path works.

static void load_gl_read_procs() noexcept {
    if (s_glReadPixels) return;
    HMODULE gl = GetModuleHandleW(L"opengl32.dll");
    if (!gl) return;
    s_glReadPixels  = reinterpret_cast<PFN_glReadPixels> (GetProcAddress(gl, "glReadPixels"));
    s_glGetIntegerv = reinterpret_cast<PFN_glGetIntegerv>(GetProcAddress(gl, "glGetIntegerv"));
}

void platform_win_gdi_present() noexcept {
    if (!s_use_gdi_present) return;
    load_gl_read_procs();
    if (!s_glReadPixels || !s_glGetIntegerv) return;

    HWND h = get_hwnd();
    if (!h) return;
    HDC hdc = GetDC(h);
    if (!hdc) return;

    RECT cr{};
    GetClientRect(h, &cr);
    const int w  = cr.right  - cr.left;
    const int h2 = cr.bottom - cr.top;
    if (w <= 0 || h2 <= 0) { ReleaseDC(h, hdc); return; }

    // Read GL viewport dimensions (may differ from client rect at DPI >1x)
    int vp[4] = {};
    s_glGetIntegerv(0x0BA2 /*GL_VIEWPORT*/, vp);
    const int glw = vp[2] > 0 ? vp[2] : w;
    const int glh = vp[3] > 0 ? vp[3] : h2;

    // Use GL_RGBA (0x1908) — universally supported on all GL drivers including
    // Parallels Metal.  GL_BGRA (0x80E1) is an extension that some Metal ICDs
    // silently accept but return zeros for.
    // Read from GL_BACK: after SwapBuffers with PFD_SWAP_COPY the back buffer
    // still holds the frame (copy, not flip), so no read-buffer switch needed.
    // The default read buffer in a double-buffered context is already GL_BACK.
    const size_t stride  = static_cast<size_t>(glw) * 4;
    const size_t bufsize = stride * static_cast<size_t>(glh);
    static std::vector<unsigned char> s_pixels;
    s_pixels.resize(bufsize);

    s_glReadPixels(0, 0, glw, glh,
        0x1908 /*GL_RGBA*/, 0x1401 /*GL_UNSIGNED_BYTE*/,
        s_pixels.data());

    // One-time pixel sanity check: log centre pixel to confirm non-zero readback
    static bool s_pixel_logged = false;
    if (!s_pixel_logged && !s_pixels.empty()) {
        s_pixel_logged = true;
        const size_t cx = (static_cast<size_t>(glw) / 2) * 4 +
                          (static_cast<size_t>(glh) / 2) * stride;
        if (cx + 3 < bufsize) {
            std::fprintf(stderr,
                "WINDIAG:gdi_present first-frame centre px R=%u G=%u B=%u A=%u (glw=%d glh=%d)\n",
                s_pixels[cx], s_pixels[cx+1], s_pixels[cx+2], s_pixels[cx+3], glw, glh);
            std::fflush(stderr);
        }
    }

    // GDI BI_RGB 32bpp layout is B G R X (little-endian), but GL_RGBA gives
    // R G B A.  Swap R<->B in-place so StretchDIBits sees the right colours.
    for (size_t i = 0; i < bufsize; i += 4) {
        unsigned char tmp  = s_pixels[i];      // R
        s_pixels[i]        = s_pixels[i + 2];  // B -> R slot
        s_pixels[i + 2]    = tmp;              // R -> B slot
        // G and A/X stay in place
    }

    // GL pixels are bottom-up; positive biHeight = bottom-up in GDI — matches.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = glw;
    bmi.bmiHeader.biHeight      = glh;   // positive = bottom-up, same as GL
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(hdc,
        0, 0, w, h2,        // dest: full client rect
        0, 0, glw, glh,     // src: full GL viewport
        s_pixels.data(), &bmi,
        DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(h, hdc);
}

}  // namespace ea_sandbox
#endif  // _WIN32

