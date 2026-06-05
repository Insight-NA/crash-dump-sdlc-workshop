# ea-sandbox — graphical demo for the EA × Insight Copilot workshop.
#
# Drives the engine_demo library (world / ECS, constraint_solver, game_loop,
# rng, frame_budget) with a 2D verlet-rope-plus-particles scene. raylib is the
# rendering interop boundary; everything outside render.cpp / app.cpp stays
# EASTL-only and allocator-aware.

add_executable(ea-sandbox
    main.cpp
    scene.cpp
    app.cpp
    headless.cpp
    telemetry.cpp
    dwm_flush.cpp
    platform_win_diag.cpp
)

# On macOS, compile the Obj-C force_visible helper that calls
# orderFrontRegardless + CanJoinAllSpaces + [window center] to guarantee
# the window appears on the active monitor regardless of GLFW's
# CGMainDisplayID()-based coordinate transform.
# platform_mac.mm provides explicit compositor flush and Cocoa event-loop
# servicing — the macOS analogue of dwm_flush.cpp + platform_win_diag.cpp.
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    target_sources(ea-sandbox PRIVATE force_visible.m platform_mac.mm)
endif()

target_include_directories(ea-sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ea-sandbox PRIVATE
    engine_demo
    raylib
    fmt::fmt
)

# raylib bundles GLFW but does not pull in macOS system frameworks transitively
# from the vcpkg-provided CMake config. Link them explicitly for desktop GL.
# On Windows the vcpkg raylib.lib also requires glfw3 to be linked separately.
if(WIN32)
    find_package(glfw3 CONFIG REQUIRED)
    # dwmapi: DwmFlush() syncs this process with the DWM compositor tick so
    # Parallels (WGL-on-Metal) actually presents rendered frames to the screen.
    # shcore: GetProcessDpiAwareness() for platform_win_diag startup telemetry.
    target_link_libraries(ea-sandbox PRIVATE glfw winmm dwmapi shcore opengl32)

    # Embed the DPI-awareness manifest so Windows presents the GL surface
    # directly without a GDI virtualisation layer on top of the client area.
    # Without PerMonitorV2 awareness, Windows stretches a white GDI bitmap over
    # the entire window, hiding the OpenGL framebuffer underneath.
    target_sources(ea-sandbox PRIVATE ea-sandbox.manifest)
endif()

if(APPLE)
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(ea-sandbox PRIVATE
        glfw
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework OpenGL"
    )
endif()

engine_demo_set_target_options(ea-sandbox)

# On macOS, build as a real .app bundle. Without a proper bundle (with
# Info.plist NSPrincipalClass=NSApplication) the dock treats the process as
# a CLI tool that happens to open a window, and the ANR watchdog fires
# aggressively — the dock badge says "Application Not Responding" even when
# the event loop is healthy. Bundling fixes this and is also how a real
# game would ship on macOS.
#
# CLI usage still works: `./ea-sandbox.app/Contents/MacOS/ea-sandbox
# --headless --seed 42 --frames 60 --out trace.csv` is unchanged. CI runs
# on Ubuntu/Windows and is unaffected.
if(APPLE)
    set_target_properties(ea-sandbox PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "ea-sandbox"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.insight.ea.sandbox"
        MACOSX_BUNDLE_EXECUTABLE_NAME "ea-sandbox"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
endif()

# raylib's headers expose RAYLIB_ALIAS macros that conflict with EASTL hashing
# helpers in some translation units; keep raylib usage scoped to render code.

# ─────────────────────────────────────────────────────────────────────────────
# test-minimal: dead-simple raylib window for macOS presentation diagnosis.
# If this also shows black, the issue is in raylib/GLFW/Metal, not our code.
# ─────────────────────────────────────────────────────────────────────────────
add_executable(test-minimal test_minimal.c)
enable_language(C)
set_target_properties(test-minimal PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(test-minimal PRIVATE raylib)
if(APPLE)
    target_link_libraries(test-minimal PRIVATE
        glfw
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation"
        "-framework OpenGL"
    )
endif()
