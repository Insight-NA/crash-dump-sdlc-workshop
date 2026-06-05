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
