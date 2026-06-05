/*
 * apps/poc/hello.c — bare-minimum raylib POC with multi-monitor fix.
 *
 * Phase 2 of the render-bisect plan.
 *
 * Phase 1.5 confirmed the loop runs (180 frames, focused=1) but the
 * window is never visible on screen.  The diagnosed root cause:
 *
 *   GLFW's _glfwTransformYCocoa() uses CGMainDisplayID() to convert
 *   window Y-coordinates.  On a multi-monitor setup where the external
 *   ultrawide (3840x1600) is NOT macOS's "main display" (the one with
 *   the menu bar), the computed window position lands OUTSIDE the
 *   physical bounds of the active monitor.  macOS reports the window
 *   as focused=1 and hidden=0 because those flags mean "key window"
 *   and "not ordered out" — not "on a visible monitor."
 *
 * Fix strategy:
 *   1. After InitWindow(), call GetCurrentMonitor() + GetMonitor*()
 *      to find the actual target monitor's position/size.
 *   2. SetWindowPosition() to explicitly center on that monitor —
 *      this overrides GLFW's CGMainDisplayID-based placement.
 *   3. Toggle FLAG_WINDOW_TOPMOST on/off to force the macOS compositor
 *      (WindowServer) to re-evaluate the window's layer order.
 *   4. SetWindowFocused() + PollInputEvents() to let Cocoa process
 *      the ordering changes before the first draw.
 *   5. Also call poc_force_visible() from force_visible.m — Obj-C
 *      nuclear option: orderFrontRegardless + CanJoinAllSpaces +
 *      [window center].  Belt-and-suspenders in case the pure C API
 *      path is insufficient.
 *
 * Also adds SetTargetFPS(60) so 180 frames = 3 visible seconds.
 *
 * Run:
 *     ./build/apps/poc/poc-hello
 *
 * Pass criterion: terminal prints monitor info + "frame=000" ...
 * "frame=150" + "exit" — AND a window appears on the ultrawide with
 * a white background, text, a moving red square, and a blue circle.
 * Both pieces of evidence required.
 */

#include <stdio.h>
#include "raylib.h"

#ifdef __APPLE__
/* Obj-C nuclear activation helper — defined in force_visible.m */
extern void poc_force_visible(void);
#endif

int main(void) {
    fprintf(stderr, "poc: boot\n");

    SetTargetFPS(60);
    InitWindow(800, 450, "poc-hello");
    fprintf(stderr, "poc: InitWindow returned, IsWindowReady=%d\n",
            (int)IsWindowReady());

    /* ----------------------------------------------------------------
     * Multi-monitor fix: GLFW uses CGMainDisplayID() for its Y-coord
     * transform, which mis-positions windows on secondary monitors.
     * Explicitly center on the monitor raylib reports for this window.
     * ---------------------------------------------------------------- */
    {
        const int mon  = GetCurrentMonitor();
        const int monW = GetMonitorWidth(mon);
        const int monH = GetMonitorHeight(mon);
        const Vector2 monPos = GetMonitorPosition(mon);
        const int winW = GetScreenWidth();
        const int winH = GetScreenHeight();
        const int cx = (int)monPos.x + (monW - winW) / 2;
        const int cy = (int)monPos.y + (monH - winH) / 2;
        fprintf(stderr,
                "poc: monitor=%d pos=(%.0f,%.0f) size=%dx%d "
                "-> centering window at (%d,%d)\n",
                mon, (double)monPos.x, (double)monPos.y,
                monW, monH, cx, cy);
        SetWindowPosition(cx, cy);
    }

    /* Toggle FLAG_WINDOW_TOPMOST on/off to force the macOS WindowServer
     * to re-evaluate layer order after the repositioning above. */
    SetWindowState(FLAG_WINDOW_TOPMOST);
    ClearWindowState(FLAG_WINDOW_TOPMOST);

    /* Re-focus and pump so Cocoa processes the ordering changes. */
    SetWindowFocused();
    PollInputEvents();
    fprintf(stderr, "poc: window repositioned, topmost-toggled, focused\n");

#ifdef __APPLE__
    /* Obj-C nuclear activation: orderFrontRegardless + CanJoinAllSpaces
     * + [window center].  Harmless no-op on Linux/Windows (not compiled
     * in via CMake's APPLE guard on force_visible.m). */
    poc_force_visible();
    fprintf(stderr, "poc: poc_force_visible() returned\n");
#endif

    int frame = 0;
    while (!WindowShouldClose() && frame < 180) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("POC: hello raylib", 240, 200, 24, DARKGRAY);
        DrawText("If you see this text + the red square, the fix worked!",
                 90, 240, 16, GREEN);
        DrawText("Window auto-exits after 180 frames (~3 sec at 60fps).",
                 120, 270, 14, GRAY);

        const int x = (frame * 4) % 800;
        DrawRectangle(x, 80, 50, 50, RED);
        DrawCircle(400, 380, 20, BLUE);

        EndDrawing();

        if (frame % 30 == 0) {
            fprintf(stderr,
                    "poc: frame=%03d focused=%d minimized=%d hidden=%d\n",
                    frame,
                    (int)IsWindowFocused(),
                    (int)IsWindowMinimized(),
                    (int)IsWindowHidden());
        }
        ++frame;
    }

    fprintf(stderr, "poc: exit (drew %d frames)\n", frame);
    CloseWindow();
    return 0;
}
