/*
 * apps/sandbox/force_visible.m — Obj-C nuclear window-visibility helper.
 *
 * Compiled only on macOS (gated in CMakeLists.txt via if(APPLE) +
 * enable_language(OBJC)).
 *
 * Root cause this fixes: GLFW's _glfwTransformYCocoa() uses
 * CGMainDisplayID() to compute window positions.  On a multi-monitor
 * setup where the external ultrawide is NOT the "main display" (the one
 * with the menu bar), GLFW places the window outside the active
 * monitor's bounds.  macOS reports focused=1/hidden=0 because those
 * flags mean "key window" and "not ordered out" — not "on screen."
 *
 * The raylib C API SetWindowPosition() call in app.cpp (the primary
 * fix) repositions the window before the first draw.  This Obj-C helper
 * is the belt-and-suspenders layer that bypasses GLFW's coordinate
 * system entirely and forces the window onto the current monitor/Space.
 *
 * Interop: app.cpp is labeled INTEROP BOUNDARY per cpp-snippets §3.
 * This .m file is an extension of that boundary — it calls Cocoa APIs
 * that raylib does not expose from C.
 */

#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include "raylib.h"
#include "force_visible.h"

void sandbox_force_visible(void) {
    NSWindow* window = (NSWindow*)GetWindowHandle();
    if (window == nil) {
        fprintf(stderr, "ea-sandbox: force_visible: GetWindowHandle() returned nil\n");
        return;
    }

    /* 1. Ensure this process is a regular foreground GUI application.
     *    The .app bundle + Info.plist NSPrincipalClass=NSApplication
     *    should already do this, but belt-and-suspenders when launched
     *    directly from a terminal. */
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    /* 2. Activate — bring to front of the activation queue. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop

    /* 3. Visible on the current Space/desktop. */
    [window setCollectionBehavior:
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* 4. Nuclear ordering — bypasses activation state. */
    [window orderFrontRegardless];

    /* 5. Center on the window's CURRENT screen (not CGMainDisplayID()). */
    [window center];

    /* 6. Accept keyboard input. */
    [window makeKeyWindow];
}
