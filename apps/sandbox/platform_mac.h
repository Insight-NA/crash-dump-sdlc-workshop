// platform_mac.h — macOS-only presentation and event-loop helpers.
// All functions are no-ops on non-Apple builds.
//
// Analogous to platform_win_diag.h: provides explicit compositor sync
// and Cocoa event-loop servicing that Apple's OpenGL-on-Metal translation
// layer does not handle reliably via FLAG_VSYNC_HINT alone.
#pragma once

namespace ea_sandbox {

#if defined(__APPLE__)

// Called once after InitWindow. Sets NSApp activation policy, configures
// the backing layer for immediate presentation, and ensures the process
// is recognized as a foreground GUI application by the WindowServer.
void platform_mac_setup() noexcept;

// Called after EndDrawing() each frame. Forces the OpenGL framebuffer
// contents to reach the display — analogous to DwmFlush() on Windows.
// On Apple Silicon's OpenGL-on-Metal path, glfwSwapBuffers may return
// without the frame actually being composited to screen.
void platform_mac_flush() noexcept;

// Pumps one iteration of Cocoa's NSRunLoop so macOS does not flag the
// process as "Application Not Responding" during heavy first-frame
// shader compilation or long draw calls. Call before the sleep limiter.
void platform_mac_service_events() noexcept;

#else

inline void platform_mac_setup() noexcept {}
inline void platform_mac_flush() noexcept {}
inline void platform_mac_service_events() noexcept {}

#endif  // __APPLE__

}  // namespace ea_sandbox
