// platform_mac.mm — macOS presentation and event-loop helpers (Obj-C++).
//
// Compiled only on macOS (gated in CMakeLists.txt via if(APPLE)).
// This is the macOS analogue of dwm_flush.cpp + platform_win_diag.cpp:
// it forces rendered frames to reach the display and keeps the Cocoa
// event loop serviced so the dock ANR watchdog never fires.
//
// MUST NOT include raylib.h — Cocoa headers conflict with raylib's
// C macro redefinitions (Rectangle, CloseWindow, etc.).
//
// INTEROP BOUNDARY per .github/instructions/cpp-snippets.instructions.md §3.

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#include <cstdio>

namespace ea_sandbox {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

void platform_mac_setup() noexcept {
    // Ensure this process is a regular foreground GUI application.
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    [NSApp activateIgnoringOtherApps:YES];

    // On macOS 10.14+ all views are layer-backed by default.  The GLFW NSGL
    // path allocates an NSOpenGLContext and attaches it to the contentView
    // via [context setView:].  For the compositor to actually display the GL
    // surface in a layer-backed view, the view's layer must be opaque and the
    // context must be told to update its drawable.
    //
    // On macOS 26 / Apple Silicon, the Metal translation layer appears to
    // require an explicit context update after the window is on screen for the
    // GL surface to be connected to the compositor's CALayer backing store.
    NSWindow* keyWin = [NSApp keyWindow];
    if (keyWin != nil) {
        NSView* contentView = [keyWin contentView];
        if (contentView != nil) {
            // GLFW's NSGL path expects to render to a non-layer-backed view.
            // On macOS 10.14+ all views are implicitly layer-backed, but we
            // can still tell the view we don't WANT explicit layer hosting —
            // this keeps the NSGL context's direct-rendering path active
            // rather than the CALayer-backed path which requires additional
            // synchronization that flushBuffer may not perform on macOS 26.
            [contentView setWantsLayer:NO];
        }
        NSOpenGLContext* ctx = [NSOpenGLContext currentContext];
        if (ctx != nil) {
            // Force context to re-bind its drawable to the view
            [ctx setView:contentView];
            [ctx update];
            // Make the context's surface opaque
            GLint opaque = 1;
            [ctx setValues:&opaque forParameter:NSOpenGLContextParameterSurfaceOpacity];
            // Set swap interval = 1 (vsync) — this changes the presentation
            // path in Apple's GL-to-Metal translation layer.
            GLint swapInterval = 1;
            [ctx setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];
            std::fprintf(stderr,
                         "ea-sandbox: platform_mac_setup — setView+update+opaque+vsync done "
                         "(view=%p layer=%p ctx=%p wantsLayer=%d)\n",
                         static_cast<void*>(contentView),
                         static_cast<void*>([contentView layer]),
                         static_cast<void*>(ctx),
                         [contentView wantsLayer]);
        }
    }

    std::fprintf(stderr, "ea-sandbox: platform_mac_setup complete\n");
}

void platform_mac_flush() noexcept {
    // On macOS 26 with Apple's GL-to-Metal translation, flushBuffer alone
    // may not present to the compositor.  We supplement with:
    // 1. glFlush() — push all GL commands to the Metal translation layer
    // 2. [context update] — ensure the drawable is still bound to the view
    // 3. CGLFlushDrawable — lower-level flush that bypasses NSOpenGLContext
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    glFlush();

    NSOpenGLContext* ctx = [NSOpenGLContext currentContext];
    if (ctx != nil) {
        [ctx update];
        // CGLFlushDrawable is the underlying C API that flushBuffer calls,
        // but calling it explicitly after update ensures the drawable
        // reference is current.
        CGLContextObj cglCtx = [ctx CGLContextObj];
        if (cglCtx != NULL) {
            CGLFlushDrawable(cglCtx);
        }
    }
#pragma clang diagnostic pop
}

void platform_mac_service_events() noexcept {
    // No-op. GLFW's glfwPollEvents handles the Cocoa event loop.
}

#pragma clang diagnostic pop

}  // namespace ea_sandbox

#endif  // __APPLE__
