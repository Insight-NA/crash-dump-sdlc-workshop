# apps/poc — diagnostic raylib proof-of-concept binaries.
#
# These are NOT shipped with the workshop deliverable. They exist solely
# to bisect the interactive-render failure on macOS / Apple Silicon by
# adding one sandbox complication at a time on top of a known-good
# raylib hello-world.
#
# Pure C, no engine_demo dependency, no telemetry, no constitutional
# constraints (this is a diagnostic harness, not workshop content).
#
# Gated separately so the default build does not produce them. Enable
# with:
#     cmake --preset default-debug -DENGINE_DEMO_BUILD_POC=ON
#
# Phase 1 ships poc-hello only. Subsequent variants are added one at a
# time as the bisection proceeds.

option(ENGINE_DEMO_BUILD_POC "Build the diagnostic raylib POC binaries" OFF)
if(NOT ENGINE_DEMO_BUILD_POC)
    return()
endif()

# The top-level project() declares LANGUAGES CXX only. The POC binaries are
# pure C against raylib, so enable C here in the directory scope.
# On Apple we also need OBJC for force_visible.m.
enable_language(C)
if(APPLE)
    enable_language(OBJC)
endif()

# ---- poc-hello -------------------------------------------------------
# Bare hello-world + multi-monitor visibility fix.  No engine_demo.
# No constitutional flags.  Plain executable (no bundle) intentional —
# we want to prove the fix works WITHOUT the .app bundle path.
#
# force_visible.m is an Obj-C file that calls Cocoa APIs directly to
# force the window onto the active monitor/Space.  It is only compiled
# on Apple because enable_language(OBJC) is scoped inside the if(APPLE)
# block below; on Linux/Windows it is simply not included.
set(POC_HELLO_SOURCES hello.c)
if(APPLE)
    list(APPEND POC_HELLO_SOURCES force_visible.m)
endif()

add_executable(poc-hello ${POC_HELLO_SOURCES})
target_link_libraries(poc-hello PRIVATE raylib)

# Pure C, raylib only. Do NOT call engine_demo_set_target_options here:
# that helper is for C++ targets and applies -fno-exceptions / -fno-rtti
# / -Werror flags that don't belong on a diagnostic harness.
set_target_properties(poc-hello PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

if(APPLE)
    # Same macOS framework block as the sandbox — vcpkg's raylib config
    # does not propagate these transitively. Without them the link
    # fails on macOS; that is independent of the bundle/Info.plist
    # question, which we explicitly want to vary in later variants.
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(poc-hello PRIVATE
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
