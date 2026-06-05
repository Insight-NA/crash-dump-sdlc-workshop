// viewport.h — coordinate-transform helpers for the sandbox renderer.
//
// Extracted from the anonymous namespace in app.cpp so that the logic can be
// unit-tested without linking against raylib. This header is included by
// app.cpp (which provides the full raylib Vector2 type) and by tests (which
// supply a minimal Vector2 stub).

#pragma once

#include <cstdint>

#ifndef VIEWPORT_VECTOR2_DEFINED
// When included from app.cpp, raylib.h is already included and provides
// Vector2.  When included from tests that don't link raylib, define a minimal
// struct here.
#if defined(RAYLIB_H)
// already defined by raylib.h — nothing to do
#else
struct Vector2 {
    float x;
    float y;
};
#define VIEWPORT_VECTOR2_DEFINED 1
#endif
#endif

namespace ea_sandbox {

struct viewport {
    int width;
    int height;
};

// World rect [-3, 3] × [-2, 2] mapped to a letterboxed pane with an 80-px
// outer margin on X and 40/60-px margins on Y.
// Y is flipped: raylib screen-space y grows downward.
[[nodiscard]] inline Vector2 world_to_screen(double wx,
                                              double wy,
                                              viewport vp) noexcept {
    const double sx_min = 80.0;
    const double sx_max = static_cast<double>(vp.width) - 80.0;
    const double sy_min = 40.0;
    const double sy_max = static_cast<double>(vp.height) - 60.0;
    const double tx = (wx + 3.0) / 6.0;
    const double ty = (wy + 2.0) / 6.0;
    Vector2 v{};
    v.x = static_cast<float>(sx_min + tx * (sx_max - sx_min));
    v.y = static_cast<float>(sy_max - ty * (sy_max - sy_min));
    return v;
}

// Inverse: screen (sx, sy) → world (wx, wy).
inline void screen_to_world(float sx,
                             float sy,
                             viewport vp,
                             double& wx,
                             double& wy) noexcept {
    const double sxmin = 80.0;
    const double sxmax = static_cast<double>(vp.width) - 80.0;
    const double symin = 40.0;
    const double symax = static_cast<double>(vp.height) - 60.0;
    const double tx = (static_cast<double>(sx) - sxmin) / (sxmax - sxmin);
    const double ty = (symax - static_cast<double>(sy)) / (symax - symin);
    wx = tx * 6.0 - 3.0;
    wy = ty * 6.0 - 2.0;
}

}  // namespace ea_sandbox
