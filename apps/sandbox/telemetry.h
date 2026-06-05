// Sandbox telemetry — full-debug runtime observability for ea-sandbox.
//
// INTEROP BOUNDARY. This translation unit is permitted to use std::chrono,
// std::fprintf, std::atomic, std::thread, and POSIX signal/backtrace APIs
// (per .github/instructions/cpp-snippets.instructions.md §3, allocator-free
// interop layers may use std). Sandbox-internal containers stay EASTL +
// allocator-aware in scene.{h,cpp}.
//
// Output: JSON-Lines (one record per line) to a file *and* a coarse
// always-on 1-line liveness tick on stderr. JSONL is grep- and jq-friendly
// and stays extensible for new record kinds (frame, event, watchdog,
// crash, raylib).
//
// Hot-path discipline: zero allocation, zero string formatting through
// fmt/EASTL. std::fprintf with explicit %.f / %llu / %s only.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace ea_sandbox {

enum class telemetry_level : std::uint8_t {
    off = 0,      // file disabled. Stderr 1-line tick still fires.
    frame = 1,    // 1 JSONL record / frame + events.
    verbose = 2,  // adds physics + render-cost detail per frame.
};

struct telemetry_options {
    telemetry_level level{telemetry_level::off};
    const char* out_path{nullptr};  // null => no file, only stderr ticks.
    bool watchdog{false};           // start the hang-watchdog jthread.
};

// Singleton-ish accessor. Returns the process-wide telemetry sink.
class telemetry {
   public:
    static telemetry& get() noexcept;

    void init(telemetry_options opts) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] telemetry_level level() const noexcept { return m_level; }
    [[nodiscard]] bool file_enabled() const noexcept { return m_file != nullptr; }

    // High-resolution clock used by all spans.
    using clock = std::chrono::steady_clock;
    [[nodiscard]] static clock::time_point now() noexcept { return clock::now(); }
    [[nodiscard]] static std::int64_t to_us(clock::duration d) noexcept;

    // ---- Per-frame frame-graph record (Phase 2) ----
    struct frame_record {
        std::uint64_t frame{0};
        std::int64_t t_input_us{0};
        std::int64_t t_step_us{0};
        std::int64_t t_draw_world_us{0};
        std::int64_t t_draw_hud_us{0};
        std::int64_t t_swap_us{0};  // EndDrawing duration.
        std::int64_t t_sleep_us{0};
        std::int64_t t_total_us{0};
        int fps{0};
        int vp_w{0};
        int vp_h{0};
        bool focused{true};
        bool minimized{false};
        bool hidden{false};
    };
    void emit_frame(const frame_record& r) noexcept;

    // ---- Physics record (Phase 4) ----
    struct physics_record {
        std::uint64_t frame{0};
        std::size_t bodies{0};
        std::size_t constraints{0};
        std::size_t particles{0};
        bool paused{false};
        const char* scene{""};
        std::uint64_t seed{0};
        std::uint64_t state_digest{0};
        std::uint32_t substeps_this_frame{0};
        double sim_time{0.0};
        double wall_time{0.0};
    };
    void emit_physics(const physics_record& r) noexcept;

    // ---- Render-cost record (Phase 5) ----
    struct render_record {
        std::uint64_t frame{0};
        std::uint32_t world_calls{0};
        std::uint32_t hud_real_refreshes{0};
        std::uint32_t hud_blits{0};
        std::uint32_t draw_text_calls{0};
    };
    void emit_render(const render_record& r) noexcept;

    // ---- Generic event record (Phases 3, 4, 6) ----
    // category: short tag like "focus", "scene", "reseed", "input",
    // "raylib", "watchdog", "crash", "boot".
    // payload: pre-formatted JSON object body without surrounding braces,
    // or nullptr for a bare {category, msg, t_us}. The caller must ensure
    // the payload is valid JSON; we wrap it with {"category":..., ...}.
    void emit_event(const char* category, const char* msg, const char* payload_json) noexcept;

    // Heartbeat for the watchdog. Main loop bumps every frame.
    void heartbeat() noexcept { m_heartbeat.fetch_add(1, std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t heartbeat_count() const noexcept {
        return m_heartbeat.load(std::memory_order_relaxed);
    }
    [[nodiscard]] clock::time_point start_time() const noexcept { return m_start; }

    // Always-on 1-line stderr tick (every ~60 frames). Cheap; no JSONL.
    void maybe_stderr_tick(std::uint64_t frame, double fps, double p50_ms, double p95_ms) noexcept;

   private:
    telemetry() = default;
    ~telemetry() = default;
    telemetry(const telemetry&) = delete;
    telemetry& operator=(const telemetry&) = delete;

    std::FILE* m_file{nullptr};
    telemetry_level m_level{telemetry_level::off};
    clock::time_point m_start{};
    std::atomic<std::uint64_t> m_heartbeat{0};
    std::uint64_t m_last_stderr_tick_frame{0};
};

// Scoped span: records a steady_clock duration into an out parameter.
class [[nodiscard]] scoped_span {
   public:
    explicit scoped_span(std::int64_t& out_us) noexcept
        : m_out(&out_us), m_start(telemetry::now()) {}

    scoped_span(const scoped_span&) = delete;
    scoped_span& operator=(const scoped_span&) = delete;
    scoped_span(scoped_span&&) = delete;
    scoped_span& operator=(scoped_span&&) = delete;

    ~scoped_span() noexcept { *m_out = telemetry::to_us(telemetry::now() - m_start); }

   private:
    std::int64_t* m_out;
    telemetry::clock::time_point m_start;
};

// Crash-handler installer. Registers SIGSEGV/SIGBUS/SIGABRT/SIGFPE handlers
// that flush the JSONL file and emit a one-line backtrace on stderr.
void install_crash_handlers() noexcept;

// Watchdog: starts a detached observer thread that emits a "hang" event
// when the heartbeat counter hasn't advanced in > threshold milliseconds.
// No-op if already running.
void start_watchdog(int threshold_ms) noexcept;
void stop_watchdog() noexcept;

}  // namespace ea_sandbox
