/*
 * apps/poc/force_visible.h — C-callable declaration for the Obj-C
 * nuclear window-visibility helper.
 *
 * On macOS, if GLFW's coordinate transform places the window off the
 * visible area (common on multi-monitor setups where the external
 * display is not CGMainDisplayID()), the window exists and reports
 * focused=1/hidden=0 but is never shown on screen.  This helper calls
 * Obj-C APIs that bypass the GLFW coordinate system entirely:
 *   - [NSApp activateIgnoringOtherApps:YES]  — front-of-queue activation
 *   - [window orderFrontRegardless]          — bypasses activation state
 *   - NSWindowCollectionBehaviorCanJoinAllSpaces — shows on current Space
 *   - [window center]                        — centers on window's screen
 *
 * Compiled only on macOS (force_visible.m); the header guard in hello.c
 * wraps the call in #ifdef __APPLE__.
 *
 * On Linux/Windows: force_visible.m is not added to the build target;
 * the #ifdef ensures no undefined-reference error.
 */

#pragma once

#ifdef __APPLE__
#ifdef __cplusplus
extern "C" {
#endif

void poc_force_visible(void);

#ifdef __cplusplus
}
#endif
#endif /* __APPLE__ */
