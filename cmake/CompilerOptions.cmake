# Compiler options for engine_demo. Hard rules:
#   - C++20
#   - no exceptions  (-fno-exceptions / /EHs-c-)
#   - no RTTI        (-fno-rtti       / /GR-)
#   - warnings as errors
#
# Apply via: engine_demo_set_target_options(<target>)

function(engine_demo_set_target_options target)
    if (MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /WX /permissive-
            /EHs-c-      # disable C++ exceptions
            /GR-         # disable RTTI
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Gw>
        )
        target_compile_definitions(${target} PRIVATE
            _HAS_EXCEPTIONS=0
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -Wpedantic
            -Wshadow -Wnon-virtual-dtor -Wold-style-cast
            -Wcast-align -Wunused -Woverloaded-virtual
            -Wconversion -Wsign-conversion -Wnull-dereference
            -Wdouble-promotion -Wformat=2
            -fno-exceptions -fno-rtti
            $<$<CONFIG:Release>:-O3>
        )
    endif()

    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

# Options for GTest-based test executables.
#
# Tests are a framework interop boundary (see AGENTS.md): GTest is built by vcpkg
# WITH C++ exceptions and RTTI enabled. Compiling the test translation units with
# /EHs-c- + _HAS_EXCEPTIONS=0 (+ /GR-) produces a different ABI for GTest's internal
# types than the prebuilt library, so the static TEST() registrars silently fail to
# populate the live registry — every run reports a vacuous "0 tests". Enabling
# exceptions + RTTI on the test exe matches GTest's ABI so registration works.
#
# The engine_demo library itself keeps the strict no-exceptions / no-RTTI flags from
# engine_demo_set_target_options(); a strict static lib linked into an
# exceptions-enabled executable is fine.
function(engine_demo_set_test_target_options target)
    if (MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /WX /permissive-
            /EHsc        # enable C++ exceptions (match GTest ABI)
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Gw>
        )
        target_compile_definitions(${target} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -Wpedantic
            -Wshadow -Wnon-virtual-dtor -Wold-style-cast
            -Wcast-align -Wunused -Woverloaded-virtual
            -Wconversion -Wsign-conversion -Wnull-dereference
            -Wdouble-promotion -Wformat=2
            $<$<CONFIG:Release>:-O3>
        )
    endif()

    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()
