# Third-party dependencies. Resolved via vcpkg manifest mode (vcpkg.json).

find_package(EASTL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# raylib is consumed only by the optional sandbox executable under apps/.
# Guarded by ENGINE_DEMO_BUILD_SANDBOX so library-only consumers can opt out.
if(ENGINE_DEMO_BUILD_SANDBOX)
    find_package(raylib CONFIG REQUIRED)
endif()

# Project-wide INTERFACE library that links EASTL. The required EASTL global
# operator new[] overrides live in src/engine_demo/eastl_overrides.cpp inside the
# engine_demo target; any consumer linking engine_demo gets them transitively.
add_library(engine_demo_eastl INTERFACE)
target_link_libraries(engine_demo_eastl INTERFACE EASTL)
add_library(engine_demo::eastl ALIAS engine_demo_eastl)
