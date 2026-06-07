function(engine_demo_add_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE
        engine_demo
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )
    engine_demo_set_target_options(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

engine_demo_add_test(test_allocator      test_allocator.cpp)
engine_demo_add_test(test_frame_budget   test_frame_budget.cpp)

# BUG-001 regression: arena allocator alignment-padded OOB write AV.
engine_demo_add_test(test_regression_BUG_001
    ../regression/BUG-001/test_regression_BUG_001.cpp)
engine_demo_add_test(test_game_loop      test_game_loop.cpp)
engine_demo_add_test(test_rng            test_rng.cpp)
engine_demo_add_test(test_world          test_world.cpp)
engine_demo_add_test(test_constraint     test_constraint.cpp)

# Sandbox renderer coordinate-transform tests.
# viewport.h is pure math (no raylib dependency) — include via project root.
engine_demo_add_test(test_sandbox_render test_sandbox_render.cpp)
target_include_directories(test_sandbox_render PRIVATE
    ${CMAKE_SOURCE_DIR}
)
