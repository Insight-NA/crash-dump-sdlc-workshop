add_library(engine_demo
    allocator.cpp
    eastl_overrides.cpp
    frame_budget.cpp
    sim/game_loop.cpp
    sim/rng.cpp
    ecs/world.cpp
    physics/constraint.cpp
)

target_include_directories(engine_demo PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(engine_demo PUBLIC
    engine_demo::eastl
)

engine_demo_set_target_options(engine_demo)
