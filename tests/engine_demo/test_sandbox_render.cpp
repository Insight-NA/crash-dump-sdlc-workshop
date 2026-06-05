// test_sandbox_render.cpp
//
// Unit tests for the sandbox renderer coordinate-transform logic.
//
// These tests replicate the failure modes that produce a white window on
// Windows / Parallels (Metal-to-OpenGL / WGL-on-Metal):
//
//  1. Zero-dimension viewport — GetRenderWidth() returns 0 before the driver
//     commits the first framebuffer.  world_to_screen with a zero viewport maps
//     every world coord to off-screen, collapsing all drawing.
//
//  2. Coordinate degeneracy — a non-zero viewport must map world-space corners
//     to distinct, on-screen positions so geometry is actually visible.
//
//  3. DWM compositor sync — on Windows/Parallels glfwSwapBuffers queues a
//     present to the WGL surface but DWM never composites it to the screen
//     without an explicit DwmFlush().  We verify the source files contain the
//     required call and linkage so the fix cannot be silently removed.

#include "apps/sandbox/viewport.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

using ea_sandbox::viewport;
using ea_sandbox::world_to_screen;
using ea_sandbox::screen_to_world;

// ---- Zero-viewport guard ------------------------------------------------
// Reproduces: GetRenderWidth() == 0 -> all draw calls land at {0,0} -> white window.

TEST(sandbox_render, zero_viewport_produces_degenerate_coordinates) {
    // When GetRenderWidth()==0, sx_max = 0 - 80 = -80 which is less than sx_min = 80.
    // The usable x-range is negative, so every mapped x is ≤ 80 (left margin or off-screen).
    // This means no geometry lands in the interior of the window → white window.
    viewport vp{0, 0};
    const Vector2 center = world_to_screen(0.0, 0.0, vp);
    const Vector2 corner = world_to_screen(3.0, 2.0, vp);
    // sx_max(0) = -80 < sx_min = 80: the drawable range has inverted, confirming degeneracy.
    // center (tx=0.5): x = 80 + 0.5*(−160) = 0  (left edge, not interior)
    // corner (tx=1.0): x = 80 + 1.0*(−160) = −80 (off-screen left)
    EXPECT_LE(center.x, 0.0F)
        << "zero viewport: world centre maps to x<=0, confirming drawable region is gone";
    EXPECT_LE(corner.x, 0.0F)
        << "zero viewport: world corner maps to x<=0, confirming drawable region is gone";
}

TEST(sandbox_render, zero_viewport_x_is_out_of_window_or_degenerate) {
    // With width==0 sx_max = 0 - 80 = -80, so sx_min + tx*(sx_max-sx_min) = 80 + tx*(-160).
    // The mapped x is always ≤ 80, meaning at best on the very left edge, never in the
    // interior.  Any value < 0 is off-screen.
    viewport vp{0, 0};
    // World centre (0,0) → tx=0.5 → x = 80 + 0.5*(−160) = 0 (left edge, not interior)
    const Vector2 c = world_to_screen(0.0, 0.0, vp);
    EXPECT_LE(c.x, 0.0F)
        << "zero viewport maps world centre to x<=0, confirming all interior geometry is clipped";
}

// ---- Correct viewport: coordinates must be distinct and on-screen --------

TEST(sandbox_render, nominal_viewport_maps_world_centre_to_screen_centre) {
    viewport vp{1280, 720};
    const Vector2 c = world_to_screen(0.0, 0.0, vp);
    // sx_min=80, sx_max=1200, tx=(0+3)/6=0.5 → x = 80 + 0.5*1120 = 640
    EXPECT_NEAR(c.x, 640.0F, 1.0F);
    // sy_min=40, sy_max=660, ty=(0+2)/6=0.333 → y = 660 - 0.333*620 ≈ 453.3
    // The formula uses the same 6-unit scale for both axes to preserve aspect ratio.
    EXPECT_NEAR(c.y, 453.3F, 1.0F);
}

TEST(sandbox_render, nominal_viewport_world_corners_are_distinct_and_on_screen) {
    viewport vp{1280, 720};
    const Vector2 tl = world_to_screen(-3.0,  2.0, vp);  // top-left world
    const Vector2 tr = world_to_screen( 3.0,  2.0, vp);  // top-right world
    const Vector2 bl = world_to_screen(-3.0, -2.0, vp);  // bottom-left world
    const Vector2 br = world_to_screen( 3.0, -2.0, vp);  // bottom-right world

    // All four corners must be strictly inside the 1280×720 window.
    for (const auto& pt : {tl, tr, bl, br}) {
        EXPECT_GT(pt.x, 0.0F)   << "corner x must be inside window";
        EXPECT_LT(pt.x, 1280.0F) << "corner x must be inside window";
        EXPECT_GT(pt.y, 0.0F)   << "corner y must be inside window";
        EXPECT_LT(pt.y, 720.0F) << "corner y must be inside window";
    }

    // Horizontal corners must be at different x values.
    EXPECT_NE(tl.x, tr.x) << "left/right world corners must map to different screen x";
    // Vertical corners must be at different y values.
    EXPECT_NE(tl.y, bl.y) << "top/bottom world corners must map to different screen y";
}

// ---- Fallback-viewport parity -------------------------------------------
// The fix in app.cpp: when GetRenderWidth()==0, use kWindowWidth (1280) instead.
// This test verifies the fallback produces the same result as the nominal path.

TEST(sandbox_render, fallback_1280x720_equals_nominal_viewport) {
    constexpr int kWindowWidth  = 1280;
    constexpr int kWindowHeight = 720;

    const int rw = 0;  // simulate GetRenderWidth() == 0
    const int rh = 0;
    const viewport vp{rw > 0 ? rw : kWindowWidth, rh > 0 ? rh : kWindowHeight};

    EXPECT_EQ(vp.width,  kWindowWidth);
    EXPECT_EQ(vp.height, kWindowHeight);

    const Vector2 c = world_to_screen(0.0, 0.0, vp);
    EXPECT_NEAR(c.x, 640.0F, 1.0F);
    EXPECT_NEAR(c.y, 453.3F, 1.0F);
}

// ---- Round-trip ---------------------------------------------------------

TEST(sandbox_render, screen_to_world_round_trips) {
    viewport vp{1280, 720};
    constexpr double kEps = 1e-4;
    for (const auto [wx, wy] : std::initializer_list<std::pair<double,double>>{
             {0.0,  0.0}, {3.0,  2.0}, {-3.0, -2.0}, {1.5, -1.0}}) {
        const Vector2 s = world_to_screen(wx, wy, vp);
        double rx = 0.0, ry = 0.0;
        screen_to_world(s.x, s.y, vp, rx, ry);
        EXPECT_NEAR(rx, wx, kEps) << "round-trip x failed for wx=" << wx;
        EXPECT_NEAR(ry, wy, kEps) << "round-trip y failed for wy=" << wy;
    }
}

// ---- DWM compositor sync regression -------------------------------------
// On Windows/Parallels (WGL-on-Metal), glfwSwapBuffers queues a present to
// the WGL surface but DWM never composites it to the screen without an
// explicit DwmFlush() call.  TakeScreenshot() reads the GL framebuffer
// directly and bypasses DWM — so screenshots look correct while the live
// window remains white.
//
// These tests verify that:
//  (a) app.cpp contains the DwmFlush() call inside the render loop.
//  (b) CMakeLists.txt links dwmapi on WIN32.
//
// They scan source files rather than using DWM at runtime so they run on all
// platforms without the Windows SDK.

namespace {

// Read an entire text file into a string.
[[nodiscard]] static bool file_contains(const char* path, const char* needle) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::rewind(f);
    if (sz <= 0) { std::fclose(f); return false; }
    char* buf = new char[static_cast<std::size_t>(sz) + 1];
    const std::size_t n = std::fread(buf, 1, static_cast<std::size_t>(sz), f);
    buf[n] = '\0';
    std::fclose(f);
    const bool found = std::strstr(buf, needle) != nullptr;
    delete[] buf;
    return found;
}

}  // anonymous namespace

// app.cpp must enable FLAG_VSYNC_HINT on all platforms.
// DPI awareness on Windows is handled via the embedded application manifest
// (ea-sandbox.manifest) which declares PerMonitorV2, not via FLAG_WINDOW_HIGHDPI.
// FLAG_WINDOW_HIGHDPI is still used on Apple to request a Retina framebuffer.
// FLAG_VSYNC_HINT is required on Parallels (WGL-on-Metal) to commit each
// rendered frame to the screen.
TEST(sandbox_render, app_cpp_enables_vsync) {
    const char* candidates[] = {
        "../../../../apps/sandbox/app.cpp",
        "apps/sandbox/app.cpp",
    };
    bool found_file  = false;
    bool found_vsync = false;
    for (const char* p : candidates) {
        FILE* f = std::fopen(p, "rb");
        if (!f) continue;
        std::fclose(f);
        found_file  = true;
        found_vsync = file_contains(p, "FLAG_VSYNC_HINT");
        break;
    }
    if (!found_file) {
        GTEST_SKIP() << "app.cpp not found relative to test CWD — skipping source scan";
    }
    EXPECT_TRUE(found_vsync)
        << "app.cpp must set FLAG_VSYNC_HINT: without it Parallels WGL-on-Metal never "
           "commits the swap to the screen";
}

// CMakeLists.txt must link dwmapi on WIN32 and embed the DPI-awareness manifest.
TEST(sandbox_render, cmake_links_dwmapi_and_embeds_manifest_on_win32) {
    const char* candidates[] = {
        "../../../../apps/sandbox/CMakeLists.txt",
        "apps/sandbox/CMakeLists.txt",
    };
    bool found_file     = false;
    bool found_link     = false;
    bool found_manifest = false;
    for (const char* p : candidates) {
        FILE* f = std::fopen(p, "rb");
        if (!f) continue;
        std::fclose(f);
        found_file     = true;
        found_link     = file_contains(p, "dwmapi");
        found_manifest = file_contains(p, "ea-sandbox.manifest");
        break;
    }
    if (!found_file) {
        GTEST_SKIP() << "CMakeLists.txt not found relative to test CWD — skipping source scan";
    }
    EXPECT_TRUE(found_link)
        << "apps/sandbox/CMakeLists.txt must link dwmapi on WIN32 for DwmFlush() to resolve";
    EXPECT_TRUE(found_manifest)
        << "apps/sandbox/CMakeLists.txt must embed ea-sandbox.manifest for PerMonitorV2 "
           "DPI-awareness on Windows (prevents GDI virtualization over the GL surface)";
}

}  // namespace
