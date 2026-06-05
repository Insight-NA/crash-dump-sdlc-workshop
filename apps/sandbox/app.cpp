// Interactive sandbox — raylib renderer. INTEROP BOUNDARY.
//
// Per .github/instructions/cpp-snippets.instructions.md §3, this translation
// unit is permitted to interact with raylib's C API. Sandbox-internal state
// outside this file remains EASTL-only and allocator-aware.

#include "app.h"

#include "scene.h"
#include "telemetry.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <raylib.h>
#include <rlgl.h>
#include <thread>

// viewport.h is included AFTER raylib.h so RAYLIB_H is already defined and
// the header's Vector2 stub is suppressed (raylib's definition wins).
#include "viewport.h"

#if defined(_WIN32)
// Forward declarations of Windows-only shims (all isolated in their own TUs
// so windows.h never shares a TU with raylib.h).
namespace ea_sandbox {
void platform_dwm_flush() noexcept;
void platform_win_pre_init() noexcept;
void platform_win_post_init() noexcept;
void platform_win_diag_startup() noexcept;
void platform_win_diag_frame(int frame) noexcept;
void platform_win_gl_finish() noexcept;
void platform_win_force_present() noexcept;
void platform_win_setup() noexcept;
void platform_win_gdi_present() noexcept;
}  // namespace ea_sandbox
#endif

#if defined(__APPLE__)
// Obj-C nuclear window-visibility helper compiled from force_visible.m.
// Defined in that TU; extern "C" linkage declared in force_visible.h.
#include "force_visible.h"
// macOS presentation + event-loop helpers (Obj-C++ in platform_mac.mm).
#include "platform_mac.h"
#endif

namespace ea_sandbox {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// raylib TraceLog callback — mirror INFO/WARN/ERROR into telemetry JSONL
// so the log captures GL init, shader compile, FBO creation, and any
// driver warnings that might explain a render anomaly.
extern "C" void raylib_trace_to_telemetry(int log_level, const char* fmt, va_list args) {
    char msg[512];
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    std::vsnprintf(msg, sizeof(msg), fmt, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    // JSON-escape: replace embedded " and \ with their escaped forms,
    // and \n with space — bounded buffer.
    char esc[512];
    std::size_t j = 0;
    for (std::size_t i = 0; msg[i] != '\0' && j + 2 < sizeof(esc); ++i) {
        const char c = msg[i];
        if (c == '"' || c == '\\') {
            esc[j++] = '\\';
            esc[j++] = c;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            esc[j++] = ' ';
        } else if (static_cast<unsigned char>(c) >= 0x20) {
            esc[j++] = c;
        }
    }
    esc[j] = '\0';

    const char* lvl_name = "info";
    switch (log_level) {
        case LOG_TRACE:
            lvl_name = "trace";
            break;
        case LOG_DEBUG:
            lvl_name = "debug";
            break;
        case LOG_INFO:
            lvl_name = "info";
            break;
        case LOG_WARNING:
            lvl_name = "warn";
            break;
        case LOG_ERROR:
            lvl_name = "error";
            break;
        case LOG_FATAL:
            lvl_name = "fatal";
            break;
        default:
            lvl_name = "info";
            break;
    }
    char payload[640];
    std::snprintf(payload, sizeof(payload), R"("level":"%s")", lvl_name);
    telemetry::get().emit_event("raylib", esc, payload);

    // Also still print to the user's stderr so the existing operator
    // experience is preserved.
    if (log_level >= LOG_INFO) {
        std::fputs("INFO: ", stderr);
        std::fputs(msg, stderr);
        std::fputc('\n', stderr);
    }
}

// viewport struct and world_to_screen/screen_to_world are now in viewport.h
// (pulled in via #include above) so they can be unit-tested without raylib.

[[nodiscard]] Color color_from_speed(double speed) noexcept {
    // C++20 designated-init style color ramp: deep blue → cyan → hot pink → white
    // Maps [0, 8] m/s with smooth hermite interpolation.
    const double t = std::clamp(speed / 8.0, 0.0, 1.0);
    const double t2 = t * t * (3.0 - 2.0 * t);  // smoothstep

    // Multi-stop gradient: blue(0) → cyan(0.33) → magenta(0.66) → white(1.0)
    double r = 0.0, g = 0.0, b = 0.0;
    if (t2 < 0.33) {
        double u = t2 / 0.33;
        r = 0.1 * u;
        g = 0.3 + 0.5 * u;
        b = 0.8 + 0.2 * u;
    } else if (t2 < 0.66) {
        double u = (t2 - 0.33) / 0.33;
        r = 0.1 + 0.8 * u;
        g = 0.8 - 0.5 * u;
        b = 1.0 - 0.2 * u;
    } else {
        double u = (t2 - 0.66) / 0.34;
        r = 0.9 + 0.1 * u;
        g = 0.3 + 0.7 * u;
        b = 0.8 + 0.2 * u;
    }
    return Color{static_cast<unsigned char>(r * 255.0),
                 static_cast<unsigned char>(g * 255.0),
                 static_cast<unsigned char>(b * 255.0),
                 255};
}

// Scale factor relative to the 1280-wide reference layout.
// All font sizes and UI positions are multiplied by this so the HUD
// stays readable at any window size.
float ui_scale(viewport vp) noexcept {
    return static_cast<float>(vp.width) / 1280.0f;
}

// Convenience: scale an integer layout value.
int sc(int v, float s) noexcept {
    return static_cast<int>(static_cast<float>(v) * s + 0.5f);
}

void draw_text_shadow(const char* s, int x, int y, int font_size, Color tint) noexcept {
    DrawText(s, x + 1, y + 1, font_size, Color{0, 0, 0, 180});
    DrawText(s, x, y, font_size, tint);
}

void draw_background(viewport vp) noexcept {
    // Deep space gradient.
    DrawRectangleGradientV(0, 0, vp.width, vp.height, Color{8, 12, 28, 255}, Color{2, 2, 6, 255});

    // Animated subtle grid that "breathes" — demonstrates real-time shader-like
    // effects without actual shaders (pure immediate-mode draw calls).
    const double t = GetTime();
    const float pulse = 0.5F + 0.5F * static_cast<float>(std::sin(t * 0.8));
    const unsigned char grid_alpha = static_cast<unsigned char>(40.0F + 30.0F * pulse);

    for (int gx = -3; gx <= 3; ++gx) {
        const Vector2 a = world_to_screen(static_cast<double>(gx), -2.5, vp);
        const Vector2 b = world_to_screen(static_cast<double>(gx), 2.5, vp);
        DrawLineEx(a, b, 1.0F, Color{60, 80, 140, grid_alpha});
    }
    for (int gy = -2; gy <= 2; ++gy) {
        const Vector2 a = world_to_screen(-3.5, static_cast<double>(gy), vp);
        const Vector2 b = world_to_screen(3.5, static_cast<double>(gy), vp);
        DrawLineEx(a, b, 1.0F, Color{60, 80, 140, grid_alpha});
    }

    // Subtle vignette overlay — darkened edges for cinematic look.
    // Draw semi-transparent rects along the borders.
    const int v = 80;  // vignette band width
    DrawRectangleGradientH(0, 0, v, vp.height, Color{0, 0, 0, 120}, Color{0, 0, 0, 0});
    DrawRectangleGradientH(vp.width - v, 0, v, vp.height, Color{0, 0, 0, 0}, Color{0, 0, 0, 120});
    DrawRectangleGradientV(0, 0, vp.width, v, Color{0, 0, 0, 100}, Color{0, 0, 0, 0});
    DrawRectangleGradientV(0, vp.height - v, vp.width, v, Color{0, 0, 0, 0}, Color{0, 0, 0, 100});
}

void draw_histogram(const engine_demo::frame_budget& fb, viewport vp) noexcept {
    const float s = ui_scale(vp);
    const int kW = sc(240, s);
    const int kH = sc(88, s);
    const int x0 = vp.width - kW - sc(20, s);
    const int y0 = vp.height - kH - sc(20, s);
    DrawRectangle(x0, y0, kW, kH, Color{0, 0, 0, 140});
    DrawRectangleLines(x0, y0, kW, kH, Color{80, 100, 140, 180});
    draw_text_shadow(
        "frame budget (ms)", x0 + sc(8, s), y0 + sc(6, s), sc(14, s), Color{180, 200, 240, 255});

    const double avg = fb.rolling_average();
    const double target = 16.667;
    const double cap = 50.0;
    const int bar_h = static_cast<int>((avg / cap) * (kH - sc(28, s)));
    Color bar_color = Color{120, 220, 120, 230};
    if (avg > 33.333) {
        bar_color = Color{220, 90, 90, 230};
    } else if (avg > 16.667) {
        bar_color = Color{220, 200, 90, 230};
    }
    DrawRectangle(x0 + sc(12, s), y0 + kH - sc(12, s) - bar_h, sc(28, s), bar_h, bar_color);

    const int y_60 = y0 + kH - sc(12, s) - static_cast<int>((target / cap) * (kH - sc(28, s)));
    const int y_30 = y0 + kH - sc(12, s) - static_cast<int>((33.333 / cap) * (kH - sc(28, s)));
    DrawLine(x0 + sc(8, s), y_60, x0 + kW - sc(8, s), y_60, Color{120, 220, 120, 200});
    DrawLine(x0 + sc(8, s), y_30, x0 + kW - sc(8, s), y_30, Color{220, 90, 90, 200});

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f ms  avg", avg);
    DrawText(buf, x0 + sc(50, s), y0 + kH - sc(22, s), sc(14, s), Color{220, 220, 240, 255});
    DrawText("60Hz", x0 + kW - sc(38, s), y_60 - sc(12, s), sc(10, s), Color{160, 230, 160, 220});
    DrawText("30Hz", x0 + kW - sc(38, s), y_30 - sc(12, s), sc(10, s), Color{230, 160, 160, 220});
}

void draw_hud(const scene& s,
              viewport vp,
              bool paused,
              double speed,
              int fps,
              std::uint64_t reseed_baseline_digest) noexcept {
    char buf[256];
    const float sc_ = ui_scale(vp);

    const double wall = s.wall_time_seconds();
    const double sim = s.sim_time_seconds();
    const double drift_ms = (wall - sim) * 1000.0;
    const std::uint64_t digest = s.state_digest();

    std::snprintf(buf, sizeof(buf), "ENGINE_DEMO showcase - %s", scene_kind_name(s.kind()));
    draw_text_shadow(buf, sc(16, sc_), sc(12, sc_), sc(22, sc_), Color{255, 240, 200, 255});

    std::snprintf(buf,
                  sizeof(buf),
                  "seed=%llu  substeps=%llu  speed=%.2fx  %s",
                  static_cast<unsigned long long>(s.seed()),
                  static_cast<unsigned long long>(s.total_substeps()),
                  speed,
                  paused ? "[PAUSED]" : "");
    draw_text_shadow(buf, sc(16, sc_), sc(40, sc_), sc(16, sc_), RAYWHITE);

    std::snprintf(
        buf, sizeof(buf), "wall=%.2fs  sim=%.2fs  drift=%+.2fms  (BUG-002)", wall, sim, drift_ms);
    Color drift_col = Color{200, 210, 220, 255};
    if (std::fabs(drift_ms) > 50.0)
        drift_col = Color{255, 220, 90, 255};
    if (std::fabs(drift_ms) > 200.0)
        drift_col = Color{255, 120, 90, 255};
    draw_text_shadow(buf, sc(16, sc_), sc(62, sc_), sc(14, sc_), drift_col);

    const double avg = s.budget().rolling_average();
    std::snprintf(buf, sizeof(buf), "frame_avg=%.2fms  fps=%d  (BUG-006)", avg, fps);
    Color fb_col = Color{200, 210, 220, 255};
    if (avg > 16.667)
        fb_col = Color{255, 220, 90, 255};
    if (avg > 33.333)
        fb_col = Color{255, 120, 90, 255};
    draw_text_shadow(buf, sc(16, sc_), sc(80, sc_), sc(14, sc_), fb_col);

    std::snprintf(buf,
                  sizeof(buf),
                  "trace_digest=%016llx  (BUG-004)",
                  static_cast<unsigned long long>(digest));
    Color dg_col = Color{200, 210, 220, 255};
    if (reseed_baseline_digest != 0 && digest != reseed_baseline_digest) {
        dg_col = Color{255, 140, 220, 255};
    }
    draw_text_shadow(buf, sc(16, sc_), sc(98, sc_), sc(14, sc_), dg_col);

    // Top-right counts panel.
    char r_buf[160];
    std::snprintf(r_buf,
                  sizeof(r_buf),
                  "bodies=%zu  edges=%zu  particles=%zu",
                  s.rope_node_count(),
                  s.constraint_edge_count(),
                  s.particle_count());
    const int rfs = sc(14, sc_);
    const int rw = MeasureText(r_buf, rfs);
    DrawRectangle(vp.width - rw - sc(32, sc_),
                  sc(8, sc_),
                  rw + sc(24, sc_),
                  sc(28, sc_),
                  Color{0, 0, 0, 140});
    draw_text_shadow(
        r_buf, vp.width - rw - sc(20, sc_), sc(16, sc_), rfs, Color{220, 230, 240, 255});

    // Bottom-left controls hint.
    const int hint_fs = sc(12, sc_);
    DrawText("1/2/3/4 scene  Space pause  S step  R reseed  H hud  T trails  P perf-bomb",
             sc(16, sc_),
             vp.height - sc(42, sc_),
             hint_fs,
             Color{170, 180, 200, 220});
    DrawText("LMB drag node  RMB burst  G gravity well  +/- speed  F1 crash  Esc quit",
             sc(16, sc_),
             vp.height - sc(24, sc_),
             hint_fs,
             Color{170, 180, 200, 220});
}

void draw_scene(const scene& s, viewport vp, bool show_trails) noexcept {
    // Particle trails — velocity-coloured with tapering width.
    if (show_trails) {
        for (std::size_t i = 0; i < s.particle_count(); ++i) {
            for (std::size_t k = 0; k + 1 < scene::kTrailLength; ++k) {
                const trail_point a = s.trail_at(i, k);
                const trail_point b = s.trail_at(i, k + 1);
                const Vector2 sa =
                    world_to_screen(static_cast<double>(a.x), static_cast<double>(a.y), vp);
                const Vector2 sb =
                    world_to_screen(static_cast<double>(b.x), static_cast<double>(b.y), vp);
                const float progress =
                    static_cast<float>(k + 1) / static_cast<float>(scene::kTrailLength);
                // Taper width from thick (recent) to thin (old).
                const float width = 2.5F * (1.0F - progress) + 0.5F;
                // Colour from trail-point velocity.
                const double seg_speed =
                    std::sqrt(static_cast<double>(a.x - b.x) * static_cast<double>(a.x - b.x) +
                              static_cast<double>(a.y - b.y) * static_cast<double>(a.y - b.y)) *
                    60.0;
                Color c = color_from_speed(seg_speed);
                c.a = static_cast<unsigned char>(progress * 180.0F);
                DrawLineEx(sa, sb, width, c);
            }
        }
    }

    // Free particles with multi-layer bloom and speed-based pulse.
    const float anim_t = static_cast<float>(GetTime());
    for (std::size_t i = 0; i < s.particle_count(); ++i) {
        const particle& p = s.particle_at(i);
        const Vector2 sp = world_to_screen(p.x, p.y, vp);
        const double speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        const Color core = color_from_speed(speed);
        const float base_r = static_cast<float>(p.radius * 60.0);

        // Speed-based size pulse (C++20 std::lerp for smooth interpolation).
        const float pulse =
            1.0F +
            0.3F * static_cast<float>(std::sin(anim_t * 4.0F + static_cast<float>(i) * 0.7F));
        const float r = base_r * pulse;

        // Outer bloom (large, very faint).
        DrawCircleV(sp, r * 4.0F, Color{core.r, core.g, core.b, 20});
        // Mid glow.
        DrawCircleV(sp, r * 2.2F, Color{core.r, core.g, core.b, 50});
        // Core.
        DrawCircleV(sp, r, core);
        // Hot center (white point for high-speed particles).
        if (speed > 3.0) {
            const float hot_alpha = static_cast<float>(std::clamp((speed - 3.0) / 5.0, 0.0, 1.0));
            DrawCircleV(
                sp, r * 0.4F, Color{255, 255, 255, static_cast<unsigned char>(hot_alpha * 200.0F)});
        }
    }

    // Constraint edges with subtle gradient glow.
    for (std::size_t e = 0; e < s.edge_count(); ++e) {
        std::size_t a = 0;
        std::size_t b = 0;
        s.edge(e, a, b);
        double ax = 0.0;
        double ay = 0.0;
        double bx = 0.0;
        double by = 0.0;
        if (!s.rope_node_position(a, ax, ay) || !s.rope_node_position(b, bx, by))
            continue;
        const Vector2 sa = world_to_screen(ax, ay, vp);
        const Vector2 sb = world_to_screen(bx, by, vp);
        // Outer glow.
        DrawLineEx(sa, sb, 4.0F, Color{100, 160, 255, 40});
        // Core edge.
        DrawLineEx(sa, sb, 2.0F, Color{200, 220, 255, 230});
    }

    // Rope/cloth nodes on top with glow ring.
    const std::size_t n = s.rope_node_count();
    const std::size_t held = s.held_index();
    for (std::size_t i = 0; i < n; ++i) {
        double x = 0.0;
        double y = 0.0;
        if (!s.rope_node_position(i, x, y))
            continue;
        const Vector2 sp = world_to_screen(x, y, vp);
        if (s.is_anchor(i)) {
            // Anchor: golden diamond shape.
            DrawCircleV(sp, 9.0F, Color{255, 200, 60, 40});
            DrawRectangleV(Vector2{sp.x - 5.0F, sp.y - 5.0F},
                           Vector2{10.0F, 10.0F},
                           Color{255, 210, 120, 255});
        } else {
            // Regular node: soft glow.
            DrawCircleV(sp, 10.0F, Color{100, 200, 255, 30});
            DrawCircleV(sp, 6.0F, Color{140, 220, 255, 80});
            DrawCircleV(sp, 4.0F, Color{200, 245, 255, 230});
        }
        if (i == held) {
            // Pulsing selection ring.
            const float p = 1.0F + 0.2F * static_cast<float>(std::sin(GetTime() * 6.0));
            DrawCircleLines(static_cast<int>(sp.x),
                            static_cast<int>(sp.y),
                            14.0F * p,
                            Color{255, 80, 220, 255});
            DrawCircleLines(static_cast<int>(sp.x),
                            static_cast<int>(sp.y),
                            16.0F * p,
                            Color{255, 80, 220, 140});
        }
    }
}

// Shockwave ring effect state — triggered by perf bomb (P key).
struct shockwave {
    double trigger_time = -10.0;  // GetTime() when triggered
    static constexpr double duration = 1.5;

    void fire() noexcept { trigger_time = GetTime(); }
    [[nodiscard]] bool active() const noexcept { return (GetTime() - trigger_time) < duration; }
    [[nodiscard]] float progress() const noexcept {
        return static_cast<float>((GetTime() - trigger_time) / duration);
    }
};
static shockwave s_shockwave{};

// Screen shake — triggered alongside perf bomb for cinematic punch.
struct screen_shake {
    double trigger_time = -10.0;
    static constexpr double duration = 0.5;
    static constexpr float intensity = 12.0F;

    void fire() noexcept { trigger_time = GetTime(); }
    [[nodiscard]] bool active() const noexcept { return (GetTime() - trigger_time) < duration; }
    [[nodiscard]] Vector2 offset() const noexcept {
        if (!active())
            return {0.0F, 0.0F};
        const float t = static_cast<float>((GetTime() - trigger_time) / duration);
        const float decay = 1.0F - t;
        const float time_f = static_cast<float>(GetTime());
        return {
            decay * intensity * static_cast<float>(std::sin(static_cast<double>(time_f) * 47.0)),
            decay * intensity * static_cast<float>(std::cos(static_cast<double>(time_f) * 53.0))};
    }
};
static screen_shake s_screen_shake{};

// Ambient floating dust motes — purely cosmetic background layer.
struct dust_mote {
    float x, y;   // screen-space position [0..1]
    float speed;  // drift speed
    float phase;  // sin offset for gentle sway
    float size;   // radius in pixels
    unsigned char alpha;
};

static constexpr std::size_t kDustCount = 60;
static dust_mote s_dust[kDustCount]{};
static bool s_dust_init = false;

void init_dust() noexcept {
    // Simple deterministic pseudo-random seeding (no <random> needed).
    for (std::size_t i = 0; i < kDustCount; ++i) {
        auto hash = static_cast<unsigned>(i * 2654435761u);
        s_dust[i].x = static_cast<float>(hash % 1000) / 1000.0F;
        hash = hash * 1664525u + 1013904223u;
        s_dust[i].y = static_cast<float>(hash % 1000) / 1000.0F;
        hash = hash * 1664525u + 1013904223u;
        s_dust[i].speed = 0.005F + static_cast<float>(hash % 100) / 10000.0F;
        hash = hash * 1664525u + 1013904223u;
        s_dust[i].phase = static_cast<float>(hash % 628) / 100.0F;
        hash = hash * 1664525u + 1013904223u;
        s_dust[i].size = 1.0F + static_cast<float>(hash % 20) / 10.0F;
        hash = hash * 1664525u + 1013904223u;
        s_dust[i].alpha = static_cast<unsigned char>(20 + hash % 40);
    }
    s_dust_init = true;
}

void draw_dust(viewport vp) noexcept {
    if (!s_dust_init)
        init_dust();
    const float t = static_cast<float>(GetTime());
    for (std::size_t i = 0; i < kDustCount; ++i) {
        auto& d = s_dust[i];
        // Gentle upward drift + horizontal sway.
        float sy = d.y - d.speed * t * 0.1F;
        sy = sy - static_cast<float>(static_cast<int>(sy));  // wrap [0,1]
        if (sy < 0.0F)
            sy += 1.0F;
        const float sx = d.x + 0.01F * std::sin(t * 0.5F + d.phase);
        const float px = sx * static_cast<float>(vp.width);
        const float py = sy * static_cast<float>(vp.height);
        DrawCircleV(Vector2{px, py}, d.size, Color{180, 200, 255, d.alpha});
    }
}

void draw_shockwave(viewport vp) noexcept {
    if (!s_shockwave.active())
        return;
    const float t = s_shockwave.progress();
    const float max_radius = static_cast<float>(vp.width) * 0.6F;
    const float radius = max_radius * t;
    const unsigned char alpha = static_cast<unsigned char>((1.0F - t) * 200.0F);
    const int cx = vp.width / 2;
    const int cy = vp.height / 2;
    // Multiple expanding rings for a shockwave look.
    for (int ring = 0; ring < 3; ++ring) {
        const float r = radius * (1.0F - static_cast<float>(ring) * 0.08F);
        if (r > 0.0F) {
            DrawCircleLines(
                cx, cy, r, Color{180, 120, 255, static_cast<unsigned char>(alpha / (ring + 1))});
        }
    }
    // Central flash on first frames.
    if (t < 0.2F) {
        const unsigned char flash = static_cast<unsigned char>((1.0F - t / 0.2F) * 60.0F);
        DrawCircleV(Vector2{static_cast<float>(cx), static_cast<float>(cy)},
                    radius * 0.3F,
                    Color{255, 200, 255, flash});
    }
}

[[noreturn]] void trigger_demo_crash() noexcept {
    // Deliberate null deref for the Session 02 crash-dump demo. Distinct from
    // the seeded BUG-001 in engine_demo::allocator so seeded bugs stay untouched.
    volatile int* p = nullptr;
    *p = 0xDEAD;
    for (;;) {
    }
}

}  // namespace

int run_interactive(std::uint64_t initial_seed,
                    scene_kind initial_scene,
                    const char* screenshot_path,
                    int screenshot_warmup_frames) noexcept {
    SetTraceLogLevel(LOG_INFO);
    // Mirror raylib's TraceLog stream into the JSONL telemetry sink so
    // GL init, shader compilation, FBO creation, and driver warnings
    // are all in one place when triaging a render anomaly.
    SetTraceLogCallback(raylib_trace_to_telemetry);
    // FLAG_WINDOW_HIGHDPI: render at the actual physical pixel count on
    //   Retina displays. Without this, raylib draws a 1x framebuffer into
    //   a 2x window and the result looks like "the window isn't rendering"
    //   — a tiny corner of pixels in a giant gray frame.
    // FLAG_VSYNC_HINT: block on display refresh inside SwapBuffers so the
    //   loop yields the CPU to Cocoa's main-thread event pump every frame
    //   instead of relying solely on SetTargetFPS's busy-ish sleep.
    // FLAG_VSYNC_HINT calls glfwSwapInterval(1) — required on Parallels
    // (WGL-on-Metal) to commit each rendered frame to the screen.
    //
    // FLAG_WINDOW_HIGHDPI is macOS-only here.  On Windows, DPI awareness is
    // declared via the embedded application manifest (ea-sandbox.manifest)
    // with dpiAwareness=PerMonitorV2.  The manifest approach is correct:
    //   - Windows stops virtualizing the window at 96 DPI.
    //   - GLFW/GL receive logical coordinates and create a 1:1 framebuffer.
    //   - GetRenderWidth() == GetScreenWidth() == kWindowWidth (1280).
    //   - No GDI DPI bitmap is placed over the client area.
    // Using FLAG_WINDOW_HIGHDPI on Windows makes raylib call
    // GLFW_SCALE_TO_MONITOR=TRUE which scales the framebuffer to physical
    // pixels, but raylib's internal glViewport stays at the logical size —
    // rendering lands in the bottom-left corner and the window appears black.
#if defined(__APPLE__)
    // On macOS, do NOT use FLAG_WINDOW_HIGHDPI — the HiDPI framebuffer
    // scaling combined with Apple's GL-on-Metal layer can cause a
    // presentation mismatch where the framebuffer has content but the
    // window composites black. Without HIGHDPI, raylib creates a 1:1
    // framebuffer matching the logical window size, avoiding the issue.
    // Also skip FLAG_VSYNC_HINT: on Apple Silicon's OpenGL-on-Metal
    // translation layer, glfwSwapInterval(1) may either not block at all
    // (frame never presented) or double-block alongside our sleep-based
    // frame limiter.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#elif defined(_WIN32)
    // Do NOT pass FLAG_VSYNC_HINT on Windows: GLFW calls wglSwapIntervalEXT(1),
    // which on Parallels (PFD_SUPPORT_COMPOSITION, WGL-on-Metal) creates a
    // double-vsync stall — WGL blocks AND DwmFlush blocks, so frames never
    // reach the screen. platform_win_setup() explicitly sets interval=0 after
    // InitWindow; DwmFlush() (called before each swap) owns all vsync timing.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#else
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
#endif
#if defined(_WIN32)
    platform_win_pre_init();
#endif
    InitWindow(kWindowWidth, kWindowHeight, "ea-sandbox - engine_demo showcase");
#if defined(_WIN32)
    platform_win_post_init();
#endif
    // Do NOT call SetTargetFPS() here. raylib implements it as a tight
    // busy-wait inside EndDrawing() that spins at 100% CPU without ever
    // yielding to Cocoa's NSApplication run loop. macOS sees the thread
    // never sleeping and fires the dock ANR watchdog ("Application Not
    // Responding") even though glfwPollEvents runs once per frame. The
    // explicit sleep-for limiter at the end of the loop provides frame
    // pacing; platform_mac_flush() forces each frame to the display.

    // Multi-monitor fix: GLFW's _glfwTransformYCocoa() uses
    // CGMainDisplayID() to convert window Y-coordinates.  On a setup
    // where the active monitor (e.g. an external ultrawide) is NOT
    // macOS's "main display" (the one with the menu bar), the computed
    // window position lands outside the physical bounds of the active
    // monitor — the window is invisible even though focused=1/hidden=0.
    // Fix: explicitly center on the monitor raylib reports for this
    // window, then toggle FLAG_WINDOW_TOPMOST to force the WindowServer
    // compositor to re-evaluate layer order, then re-focus.
#if defined(__APPLE__) && 0  // DISABLED: window manipulation breaks GL-on-Metal presentation
    {
        const int mon = GetCurrentMonitor();
        const int mon_w = GetMonitorWidth(mon);
        const int mon_h = GetMonitorHeight(mon);
        const Vector2 mon_pos = GetMonitorPosition(mon);
        const int cx = static_cast<int>(mon_pos.x) + (mon_w - kWindowWidth) / 2;
        const int cy = static_cast<int>(mon_pos.y) + (mon_h - kWindowHeight) / 2;
        SetWindowPosition(cx, cy);
        SetWindowState(FLAG_WINDOW_TOPMOST);
        ClearWindowState(FLAG_WINDOW_TOPMOST);
    }
#endif
    // Force the window to the front — `open` doesn't always promote a
    // freshly-launched bundle on multi-monitor/multi-space setups.
    SetWindowFocused();
    PollInputEvents();
#if defined(__APPLE__) && 0  // DISABLED: force_visible disrupts GL context surface binding
    // Obj-C nuclear activation: orderFrontRegardless + CanJoinAllSpaces +
    // [window center].  Belt-and-suspenders layer on top of the
    // SetWindowPosition centering above.  Harmless if the window is
    // already visible.
    sandbox_force_visible();
#endif

    if (!IsWindowReady()) {
        std::fprintf(stderr, "ea-sandbox: failed to open window\n");
        return 1;
    }

#if defined(_WIN32)
    // Run deep HWND/DWM/WGL/DPI diagnostics now that the window exists.
    platform_win_diag_startup();
    // One-time setup: disable DWM transitions, uncloak if needed, force opaque.
    platform_win_setup();
#endif

#if defined(__APPLE__)
    // macOS presentation setup: configure layer-backed view, bind context,
    // set surface opacity. Must be called after InitWindow.
    platform_mac_setup();
#endif

    scene s{initial_seed};
    if (initial_scene != scene_kind::rope) {
        s.switch_scene(initial_scene);
    }
    bool paused = false;
    bool single_step = false;
    bool show_hud = true;
    bool show_trails = true;
    double speed = 1.0;
    int frames_drawn = 0;
    std::uint64_t reseed_baseline = 0;

    // The HUD is direct-drawn each frame.  We previously cached it into a
    // RenderTexture2D refreshed every 6 frames as an ANR mitigation (raylib's
    // GetGlyphIndex is a per-character linear scan over 224 default-font
    // glyphs).  The actual ANR root cause was raylib's SetTargetFPS busy-wait
    // — fixed by the sleep_for-based limiter below — so the cache became
    // unnecessary complexity that introduced render-texture lifecycle and
    // framebuffer-binding hazards (intermittent black-window symptoms on
    // Apple Silicon Retina).  Direct-drawing matches the standard raylib
    // idiom and is fast enough at 60 Hz with a real frame limiter.

    // ---- Telemetry rolling stats for stderr 1-line tick ----
    constexpr std::size_t kFrameWindow = 120;
    double frame_ms_window[kFrameWindow] = {};
    std::size_t frame_ms_idx = 0;
    std::size_t frame_ms_count = 0;

    // Window/input transition trackers.
    bool last_focused = IsWindowFocused();
    bool last_minimized = IsWindowMinimized();
    bool last_hidden = IsWindowHidden();
    int last_vp_w = GetScreenWidth();
    int last_vp_h = GetScreenHeight();

    // HUD-cache stats accumulators.
    std::uint32_t hud_real_refreshes_total = 0;
    std::uint32_t hud_blits_total = 0;

    // Self-measured frame dt.  We do NOT use raylib's GetFrameTime() / GetFPS():
    // on Apple Silicon (OpenGL-on-Metal, no SetTargetFPS) raylib's CORE.Time.frame
    // stays ~0 because SwapBuffers doesn't block on vsync and the external
    // sleep_for we use as the frame limiter is not captured by raylib's update/draw
    // accounting.  GetFrameTime() == 0 made dt == 0 and froze physics entirely.
    // Measure the wall-clock gap between consecutive iterations directly.
    auto last_frame_begin = telemetry::clock::now();
    int self_fps = 0;
    int self_fps_counter = 0;
    auto self_fps_window_begin = last_frame_begin;

    {
        char payload[256];
        std::snprintf(payload,
                      sizeof(payload),
                      R"("vp_w":%d,"vp_h":%d,"seed":%llu,"scene":"%s")",
                      last_vp_w,
                      last_vp_h,
                      static_cast<unsigned long long>(initial_seed),
                      scene_kind_name(s.kind()));
        telemetry::get().emit_event("window", "ready", payload);
    }

    while (!WindowShouldClose()) {
        const auto t_frame_begin = telemetry::clock::now();
        // Use LOGICAL (screen) dimensions for the viewport — raylib's
        // drawing APIs (DrawCircleV, DrawLineEx, DrawText, etc.) operate
        // in logical coordinates on all platforms.  On macOS with
        // FLAG_WINDOW_HIGHDPI, GetRenderWidth() returns the physical
        // framebuffer size (e.g. 5120 on a Retina display for a 1280
        // logical window), but drawing at physical coordinates pushes
        // everything off the logical viewport, producing a black screen
        // with content squeezed into the bottom-left corner.
        //
        // On Windows (no FLAG_WINDOW_HIGHDPI), GetScreenWidth() ==
        // GetRenderWidth(), so this is a no-op change there.
        //
        // Guard against returning 0 on the first few frames under
        // Parallels/Metal (compositor hasn't committed the framebuffer).
        const int rw = GetScreenWidth();
        const int rh = GetScreenHeight();
        viewport vp{rw > 0 ? rw : kWindowWidth, rh > 0 ? rh : kWindowHeight};

        // Window-state transitions.
        const bool cur_focused = IsWindowFocused();
        const bool cur_minimized = IsWindowMinimized();
        const bool cur_hidden = IsWindowHidden();
        if (cur_focused != last_focused) {
            telemetry::get().emit_event(
                "window", cur_focused ? "focus_gained" : "focus_lost", nullptr);
            last_focused = cur_focused;
        }
        if (cur_minimized != last_minimized) {
            telemetry::get().emit_event(
                "window", cur_minimized ? "minimized" : "restored", nullptr);
            last_minimized = cur_minimized;
        }
        if (cur_hidden != last_hidden) {
            telemetry::get().emit_event("window", cur_hidden ? "hidden" : "shown", nullptr);
            last_hidden = cur_hidden;
        }
        if (vp.width != last_vp_w || vp.height != last_vp_h) {
            char payload[128];
            std::snprintf(payload,
                          sizeof(payload),
                          R"("from_w":%d,"from_h":%d,"to_w":%d,"to_h":%d)",
                          last_vp_w,
                          last_vp_h,
                          vp.width,
                          vp.height);
            telemetry::get().emit_event("window", "resize", payload);
            last_vp_w = vp.width;
            last_vp_h = vp.height;
        }

        telemetry::frame_record fr{};
        fr.frame = static_cast<std::uint64_t>(frames_drawn);
        fr.vp_w = vp.width;
        fr.vp_h = vp.height;
        fr.focused = cur_focused;
        fr.minimized = cur_minimized;
        fr.hidden = cur_hidden;

        // ---- Input ----
        // Poll exactly once per frame, here, before reading any input state.
        // raylib clears IsKeyPressed/IsMouseButtonPressed on each PollInputEvents
        // call, so multiple polls per frame would swallow events before the input
        // block below can see them — causing the intermittent "input not working"
        // symptom. All other PollInputEvents calls later in the loop are removed.
        PollInputEvents();
        std::uint32_t key_events = 0;
        std::uint32_t mouse_events = 0;
        {
            scoped_span span(fr.t_input_us);
            if (IsKeyPressed(KEY_ONE)) {
                s.switch_scene(scene_kind::rope);
                ++key_events;
                telemetry::get().emit_event("scene", "switch", R"("to":"rope")");
            }
            if (IsKeyPressed(KEY_TWO)) {
                s.switch_scene(scene_kind::pendulum_tower);
                ++key_events;
                telemetry::get().emit_event("scene", "switch", R"("to":"pendulum_tower")");
            }
            if (IsKeyPressed(KEY_THREE)) {
                s.switch_scene(scene_kind::cloth);
                ++key_events;
                telemetry::get().emit_event("scene", "switch", R"("to":"cloth")");
            }
            if (IsKeyPressed(KEY_FOUR)) {
                s.switch_scene(scene_kind::particle_storm);
                ++key_events;
                telemetry::get().emit_event("scene", "switch", R"("to":"particle_storm")");
            }
            if (IsKeyPressed(KEY_SPACE)) {
                paused = !paused;
                ++key_events;
                telemetry::get().emit_event("input", paused ? "paused" : "resumed", nullptr);
            }
            if (IsKeyPressed(KEY_S)) {
                single_step = paused;
                ++key_events;
            }
            if (IsKeyPressed(KEY_H)) {
                show_hud = !show_hud;
                ++key_events;
            }
            if (IsKeyPressed(KEY_T)) {
                show_trails = !show_trails;
                ++key_events;
            }
            if (IsKeyPressed(KEY_R)) {
                ++key_events;
                // Reseed without changing the seed input — visualizes BUG-004
                // (hash-map iteration order in constraint_solver).
                reseed_baseline = s.state_digest();
                s.reseed(s.seed());
                char payload[160];
                std::snprintf(payload,
                              sizeof(payload),
                              R"("pre_digest":"%016llx","post_digest":"%016llx","seed":%llu)",
                              static_cast<unsigned long long>(reseed_baseline),
                              static_cast<unsigned long long>(s.state_digest()),
                              static_cast<unsigned long long>(s.seed()));
                telemetry::get().emit_event("scene", "reseed", payload);
            }
            if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL)) {
                speed = (speed < 4.0) ? speed * 1.25 : speed;
                ++key_events;
            }
            if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)) {
                speed = (speed > 0.125) ? speed / 1.25 : speed;
                ++key_events;
            }
            if (IsKeyPressed(KEY_F1)) {
                telemetry::get().emit_event("input", "f1_crash_requested", nullptr);
                trigger_demo_crash();
            }

            // P = "Performance bomb": spawn 5000 particles to intentionally
            // blow up the frame budget.  Use this to demo the live perf
            // regression → diagnose → fix workflow with Copilot.
            if (IsKeyPressed(KEY_P)) {
                ++key_events;
                s_shockwave.fire();
                s_screen_shake.fire();
                // Scatter bursts across the viewport for visual impact.
                for (int bi = 0; bi < 10; ++bi) {
                    double bx = -2.0 + 4.0 * (static_cast<double>(bi) / 9.0);
                    double by = (bi % 2 == 0) ? 0.5 : -0.5;
                    s.spawn_particle_burst(bx, by, 500);
                }
                telemetry::get().emit_event("input", "perf_bomb", R"("particles_added":5000)");
            }

            // Mouse drag.
            const Vector2 m = GetMousePosition();
            double mwx = 0.0;
            double mwy = 0.0;
            screen_to_world(m.x, m.y, vp, mwx, mwy);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ++mouse_events;
                const bool grabbed = s.grab_nearest_rope_node(mwx, mwy, 0.4);
                if (grabbed) {
                    char payload[128];
                    std::snprintf(payload,
                                  sizeof(payload),
                                  R"("wx":%.3f,"wy":%.3f,"node":%zu)",
                                  mwx,
                                  mwy,
                                  s.held_index());
                    telemetry::get().emit_event("input", "node_grabbed", payload);
                }
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && s.has_held_node()) {
                s.drag_held_node(mwx, mwy);
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                ++mouse_events;
                if (s.has_held_node()) {
                    telemetry::get().emit_event("input", "node_released", nullptr);
                }
                s.release_held_node();
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                ++mouse_events;
                s.spawn_particle_burst(mwx, mwy, 12);
                char payload[96];
                std::snprintf(
                    payload, sizeof(payload), R"("wx":%.3f,"wy":%.3f,"count":12)", mwx, mwy);
                telemetry::get().emit_event("input", "burst_spawned", payload);
            }

            // G held = gravity well at mouse cursor.
            // Applies a per-frame radial force toward the mouse in world space.
            // Demonstrates Copilot generating gameplay-physics code from English.
            if (IsKeyDown(KEY_G)) {
                constexpr double gravity_strength = 8.0;
                constexpr double gravity_radius = 2.5;
                for (std::size_t i = 0; i < s.particle_count(); ++i) {
                    auto& p = s.particle_mut(i);
                    const double dx = mwx - p.x;
                    const double dy = mwy - p.y;
                    const double dist = std::sqrt(dx * dx + dy * dy) + 0.01;
                    if (dist < gravity_radius) {
                        // Inverse-distance attraction (capped to avoid explosions).
                        const double force = gravity_strength / (dist * dist + 0.1);
                        const double cap = 20.0;
                        const double f = (force > cap) ? cap : force;
                        p.vx += (dx / dist) * f * (1.0 / 60.0);
                        p.vy += (dy / dist) * f * (1.0 / 60.0);
                    }
                }
            }
        }

        // ---- Step ----
        // Self-measured dt from the monotonic clock (see comment by
        // last_frame_begin above for why we don't use GetFrameTime()).
        // First frame yields ~0 (one skipped step, harmless); every
        // subsequent frame yields the true wall-clock gap including the
        // sleep-based frame limiter.
        const double measured_dt =
            std::chrono::duration<double>(t_frame_begin - last_frame_begin).count();
        last_frame_begin = t_frame_begin;
        // Self-measured FPS: count frames per rolling 1-second window.
        ++self_fps_counter;
        if (std::chrono::duration<double>(t_frame_begin - self_fps_window_begin).count() >= 1.0) {
            self_fps = self_fps_counter;
            self_fps_counter = 0;
            self_fps_window_begin = t_frame_begin;
        }
        // Clamp the per-frame dt so a window drag, focus loss, or app switch
        // (which can make the measured gap a multi-second spike) cannot
        // queue hundreds of substeps in a single frame and freeze the UI.
        // 0.1s ≈ 6 substeps at 60Hz — generous, but bounded.
        double frame_dt = measured_dt * speed;
        if (frame_dt > 0.1) {
            char payload[128];
            std::snprintf(payload, sizeof(payload), R"("raw_dt":%.6f,"clamped":0.1)", frame_dt);
            telemetry::get().emit_event("loop", "dt_clamped", payload);
            frame_dt = 0.1;
        }
        // In screenshot mode, force a deterministic fixed-step advance so
        // the captured frame always shows physics in motion.
        const double dt =
            (screenshot_path != nullptr) ? scene::kFixedStepSeconds * speed : frame_dt;
        std::uint32_t substeps_this_frame = 0;
        {
            scoped_span span(fr.t_step_us);
            if (!paused) {
                substeps_this_frame = s.step(dt);
            } else if (single_step) {
                substeps_this_frame = s.step(scene::kFixedStepSeconds);
                single_step = false;
            }
        }

        // ---- Render ----
        std::uint32_t hud_refreshes_this_frame = 0;
        std::uint32_t hud_blits_this_frame = 0;
        BeginDrawing();
        ClearBackground(Color{8, 10, 16, 255});

        // Screen shake: offset all subsequent draws via rlgl translate.
        // Resets at frame end (EndDrawing resets the matrix stack).
        const Vector2 shake = s_screen_shake.offset();
        if (shake.x != 0.0F || shake.y != 0.0F) {
            rlTranslatef(shake.x, shake.y, 0.0F);
        }

        {
            scoped_span span(fr.t_draw_world_us);
            draw_background(vp);
            draw_dust(vp);

            // World floor reference.
            DrawLineEx(world_to_screen(-3.0, -2.0, vp),
                       world_to_screen(3.0, -2.0, vp),
                       2.0F,
                       Color{60, 70, 100, 220});

            draw_scene(s, vp, show_trails);
            draw_shockwave(vp);

            // Gravity well visual indicator when G is held.
            if (IsKeyDown(KEY_G)) {
                const Vector2 mp = GetMousePosition();
                const float pulse = 0.7F + 0.3F * static_cast<float>(std::sin(GetTime() * 5.0));
                DrawCircleV(mp, 40.0F * pulse, Color{160, 80, 255, 30});
                DrawCircleV(mp, 20.0F * pulse, Color{200, 120, 255, 50});
                DrawCircleLines(static_cast<int>(mp.x),
                                static_cast<int>(mp.y),
                                60.0F * pulse,
                                Color{180, 100, 255, 80});
            }
        }
        if (show_hud) {
            scoped_span span(fr.t_draw_hud_us);
            draw_hud(s, vp, paused, speed, self_fps, reseed_baseline);
            draw_histogram(s.budget(), vp);
            hud_refreshes_this_frame = 1;
            hud_blits_this_frame = 1;
            ++hud_real_refreshes_total;
            ++hud_blits_total;
        }
        {
            scoped_span span(fr.t_swap_us);
#if defined(_WIN32)
            // Flush all pending GL commands to the GPU before SwapBuffers.
            // On Parallels (WGL-on-Metal, PFD_SWAP_COPY) the Metal bridge may
            // not have completed the back-buffer render by the time SwapBuffers
            // copies it to the front.  glFinish() ensures the GPU is idle.
            platform_win_gl_finish();
#endif
            EndDrawing();  // calls SwapBuffers -- submits frame to DWM
#if defined(__APPLE__)
            // Supplement EndDrawing's flushBuffer with explicit context update
            // + CGLFlushDrawable to force the compositor to pick up the frame.
            platform_mac_flush();
#endif
#if defined(_WIN32)
            // DwmFlush() *after* SwapBuffers: blocks until DWM has composited
            // and presented the frame we just submitted.  Calling it before
            // SwapBuffers (previous order) waited for the *prior* frame and
            // left the new frame sitting in the redirection bitmap uncollected.
            platform_dwm_flush();
            // On Parallels (PFD_SUPPORT_COMPOSITION, WGL-on-Metal), DWM's
            // GDI redirection bitmap is never filled by the Metal GL layer.
            // platform_win_gdi_present() reads the GL framebuffer via
            // glReadPixels and blits it into the window DC with StretchDIBits,
            // writing into the surface DWM actually composites.
            platform_win_gdi_present();
            platform_win_force_present();
            platform_win_diag_frame(frames_drawn);
#endif
        }

        // Manual sleep-based frame limiter.
        // FLAG_VSYNC_HINT (OpenGL-on-Metal) may not actually block in
        // SwapBuffers, leaving the loop at 100 % CPU and triggering the
        // macOS dock ANR watchdog.  A real sleep yields the thread to
        // Cocoa's event pump — the critical thing the ANR watchdog checks.
        // We measure the elapsed work this frame ourselves rather than
        // trusting GetFrameTime(), which is unreliable on this platform.

        {
            scoped_span span(fr.t_sleep_us);
            constexpr double kTargetFrameSeconds = 1.0 / 60.0;
            const double work_elapsed =
                std::chrono::duration<double>(telemetry::clock::now() - t_frame_begin).count();
            if (work_elapsed < kTargetFrameSeconds) {
                const auto sleep_ms =
                    static_cast<int>((kTargetFrameSeconds - work_elapsed) * 1000.0);
                if (sleep_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }
            }
        }

        // ---- Telemetry emit ----
        fr.t_total_us = telemetry::to_us(telemetry::clock::now() - t_frame_begin);
        fr.fps = self_fps;
        telemetry::get().emit_frame(fr);

        // Per-frame physics record (verbose only).
        if (telemetry::get().level() >= telemetry_level::verbose) {
            telemetry::physics_record pr{};
            pr.frame = fr.frame;
            pr.bodies = s.rope_node_count();
            pr.constraints = s.constraint_edge_count();
            pr.particles = s.particle_count();
            pr.paused = paused;
            pr.scene = scene_kind_name(s.kind());
            pr.seed = s.seed();
            pr.state_digest = s.state_digest();
            pr.substeps_this_frame = substeps_this_frame;
            pr.sim_time = s.sim_time_seconds();
            pr.wall_time = s.wall_time_seconds();
            telemetry::get().emit_physics(pr);

            telemetry::render_record rr{};
            rr.frame = fr.frame;
            rr.world_calls = 1;
            rr.hud_real_refreshes = hud_refreshes_this_frame;
            rr.hud_blits = hud_blits_this_frame;
            rr.draw_text_calls = 0;  // HUD-cached; no per-frame text calls.
            telemetry::get().emit_render(rr);
        }

        // Input-activity event when non-zero.
        if ((key_events | mouse_events) != 0) {
            char payload[96];
            std::snprintf(
                payload, sizeof(payload), R"("keys":%u,"mouse":%u)", key_events, mouse_events);
            telemetry::get().emit_event("input", "activity", payload);
        }

        // Rolling p50/p95 over a 120-frame window for the stderr tick.
        {
            const double frame_ms = static_cast<double>(fr.t_total_us) / 1000.0;
            frame_ms_window[frame_ms_idx] = frame_ms;
            frame_ms_idx = (frame_ms_idx + 1) % kFrameWindow;
            if (frame_ms_count < kFrameWindow)
                ++frame_ms_count;

            // Insertion-sort copy for percentiles. kFrameWindow is small (120).
            double sorted[kFrameWindow];
            for (std::size_t i = 0; i < frame_ms_count; ++i)
                sorted[i] = frame_ms_window[i];
            for (std::size_t i = 1; i < frame_ms_count; ++i) {
                double v = sorted[i];
                std::size_t j = i;
                while (j > 0 && sorted[j - 1] > v) {
                    sorted[j] = sorted[j - 1];
                    --j;
                }
                sorted[j] = v;
            }
            const double p50 = frame_ms_count > 0 ? sorted[frame_ms_count / 2] : 0.0;
            const double p95 = frame_ms_count > 0 ? sorted[(frame_ms_count * 95) / 100] : 0.0;
            telemetry::get().maybe_stderr_tick(fr.frame, static_cast<double>(fr.fps), p50, p95);
        }

        telemetry::get().heartbeat();

        ++frames_drawn;
        if (screenshot_path != nullptr && frames_drawn >= screenshot_warmup_frames) {
            TakeScreenshot(screenshot_path);
            std::fprintf(stdout,
                         "ea-sandbox: wrote screenshot to %s after %d frames\n",
                         screenshot_path,
                         frames_drawn);
            break;
        }
    }

    {
        char payload[160];
        std::snprintf(payload,
                      sizeof(payload),
                      R"("frames":%d,"hud_refreshes":%u,"hud_blits":%u)",
                      frames_drawn,
                      hud_real_refreshes_total,
                      hud_blits_total);
        telemetry::get().emit_event("loop", "exit", payload);
    }

    CloseWindow();
    return 0;
}

}  // namespace ea_sandbox
