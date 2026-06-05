/*
 * test_minimal.c — Dead-simple raylib window test.
 *
 * Purpose: Determine if raylib + GLFW can present ANYTHING on macOS 26 / M4 Pro
 * with the current vcpkg build. If this also shows black, the issue is in
 * raylib/GLFW/Metal-translation, not in our sandbox code.
 *
 * Build: cmake --build build --target test-minimal
 * Run:   ./build/apps/sandbox/test-minimal
 */

#include "raylib.h"

#include <stdio.h>

int main(void) {
    /* Absolutely minimal — no custom flags, no HiDPI, no manipulation */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "test-minimal: raylib macOS presentation test");
    SetTargetFPS(60);

    printf("test-minimal: window opened, screen=%dx%d render=%dx%d\n",
           GetScreenWidth(),
           GetScreenHeight(),
           GetRenderWidth(),
           GetRenderHeight());

    int frame = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RED);
        DrawRectangle(100, 100, 200, 150, BLUE);
        DrawCircle(500, 250, 80, GREEN);
        DrawText("If you can read this, presentation works!", 120, 30, 20, WHITE);
        DrawText(TextFormat("Frame: %d", frame), 10, 420, 20, YELLOW);
        DrawFPS(700, 10);
        EndDrawing();
        frame++;
        /* After 60 frames, take a screenshot and exit */
        if (frame == 60) {
            TakeScreenshot("test_minimal_shot.png");
            printf("test-minimal: screenshot saved after %d frames\n", frame);
            break;
        }
    }

    CloseWindow();
    return 0;
}
