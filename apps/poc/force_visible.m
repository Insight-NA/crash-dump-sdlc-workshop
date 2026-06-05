/*
 * apps/poc/force_visible.m — Obj-C nuclear window-visibility helper.
 *
 * Compiled only on macOS (gated in CMakeLists.txt via if(APPLE)).
 *
 * Calls Obj-C APIs that bypass GLFW's coordinate system entirely so
 * the window appears on whatever physical screen the user is looking at,
 * regardless of which monitor macOS considers "CGMainDisplayID()".
 *
 * Interop note: raylib's GetWindowHandle() on macOS returns the NSWindow*
 * (sourced from glfwGetCocoaWindow via rlGetWindowHandle).  We cast it
 * to NSWindow* and call Cocoa methods directly.
 */

#import <Cocoa/Cocoa.h>
#include "raylib.h"
#include "force_visible.h"

void poc_force_visible(void) {
    NSWindow* window = (NSWindow*)GetWindowHandle();
    if (window == nil) {
        fprintf(stderr, "poc: force_visible: GetWindowHandle() returned nil\n");
        return;
    }

    /* 1. Ensure this process is a regular foreground GUI application.
     *    When launched from a terminal (including VS Code's integrated
     *    terminal), the activation policy may be NSApplicationActivationPolicyAccessory
     *    or NSApplicationActivationPolicyProhibited, which suppresses
     *    window compositing. */
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    /* 2. Activate the app — bring it to the front of the activation queue.
     *    activateIgnoringOtherApps:YES works on all macOS versions; the
     *    macOS 14+ [NSApp activate] variant isn't available in all SDKs. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop

    /* 3. CanJoinAllSpaces: makes the window visible on the CURRENT Space /
     *    desktop even if the process was launched from a different Space.
     *    FullScreenAuxiliary: prevents the window from going full-screen
     *    on its own. */
    [window setCollectionBehavior:
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* 4. orderFrontRegardless: orders the window to the front of the
     *    screen list WITHOUT checking activation state.  This is the
     *    nuclear option — it works even if NSApp is not active. */
    [window orderFrontRegardless];

    /* 5. Center on the window's CURRENT screen (not CGMainDisplayID()).
     *    [NSWindow center] uses self.screen which reflects the monitor
     *    the window is closest to — independent of GLFW's coordinate
     *    transform. */
    [window center];

    /* 6. Make it the key window so keyboard input works. */
    [window makeKeyWindow];

    fprintf(stderr,
            "poc: force_visible: activation=Regular, orderFrontRegardless, "
            "CanJoinAllSpaces, center, makeKeyWindow done\n");
}
