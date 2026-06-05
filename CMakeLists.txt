cmake_minimum_required(VERSION 3.28)

project(engine_demo
    VERSION 0.1.0
    DESCRIPTION "EA x Insight Copilot Workshop demo workspace"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Always emit compile_commands.json for clang-tidy + IDE tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sandbox executable (apps/sandbox/ea-sandbox). ON by default; set OFF to skip
# raylib dependency on restricted CI runners.
option(ENGINE_DEMO_BUILD_SANDBOX "Build apps/sandbox graphical demo (requires raylib)" ON)

include(cmake/CompilerOptions.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(ENGINE_DEMO_BUILD_SANDBOX)
    add_subdirectory(apps)
endif()

enable_testing()
add_subdirectory(tests)
