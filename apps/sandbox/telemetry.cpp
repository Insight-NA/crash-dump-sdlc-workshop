// Sandbox telemetry — implementation.
//
// INTEROP BOUNDARY (see telemetry.h header banner).

#include "telemetry.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#include <unistd.h>
#endif

// STDERR_FILENO is a POSIX constant (value 2). Define it on Windows so the
// crash-handler path compiles; safe_write() ignores the fd on Windows anyway.
#if !defined(STDERR_FILENO)
#define STDERR_FILENO 2
#endif

namespace ea_sandbox {

namespace {

// Watchdog state. Lives at file scope so signal handlers can reach it.
std::atomic<bool> g_watchdog_running{false};
std::atomic<bool> g_watchdog_stop{false};
int g_watchdog_threshold_ms = 2000;

// Crash-handler reentrancy guard.
std::atomic<int> g_crash_in_progress{0};

[[nodiscard]] std::int64_t monotonic_us_since(telemetry::clock::time_point start) noexcept {
    return telemetry::to_us(telemetry::now() - start);
}

// async-signal-safe write. Used inside crash handlers.
void safe_write(int fd, const char* s) noexcept {
#if defined(__APPLE__) || defined(__linux__)
    ::write(fd, s, std::strlen(s));
#else
    (void)fd;
    std::fputs(s, stderr);
#endif
}

}  // namespace

telemetry& telemetry::get() noexcept {
    static telemetry instance;
    return instance;
}

std::int64_t telemetry::to_us(clock::duration d) noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

void telemetry::init(telemetry_options opts) noexcept {
    m_level = opts.level;
    m_start = clock::now();
    m_heartbeat.store(0, std::memory_order_relaxed);
    m_last_stderr_tick_frame = 0;

    if (opts.out_path != nullptr && m_level != telemetry_level::off) {
        m_file = std::fopen(opts.out_path, "wb");
        if (m_file == nullptr) {
            std::fprintf(stderr,
                         "ea-sandbox telemetry: failed to open %s (errno=%d %s)\n",
                         opts.out_path,
                         errno,
                         std::strerror(errno));
        } else {
            // Line-buffered so a hard kill still leaves recent records on disk.
            std::setvbuf(m_file, nullptr, _IOLBF, 0);
        }
    }

    // Boot record always emitted (even at level=off) when file exists.
    // Useful as the file's first line; downstream tooling can parse it.
    if (m_file != nullptr) {
        std::fprintf(m_file,
                     R"({"kind":"boot","t_us":0,"level":%u,"watchdog":%s})"
                     "\n",
                     static_cast<unsigned>(m_level),
                     opts.watchdog ? "true" : "false");
    }
}

void telemetry::shutdown() noexcept {
    if (m_file != nullptr) {
        std::fprintf(m_file,
                     R"({"kind":"shutdown","t_us":%lld})"
                     "\n",
                     static_cast<long long>(monotonic_us_since(m_start)));
        std::fclose(m_file);
        m_file = nullptr;
    }
}

void telemetry::emit_frame(const frame_record& r) noexcept {
    if (m_file == nullptr || m_level < telemetry_level::frame) {
        return;
    }
    std::fprintf(m_file,
                 R"({"kind":"frame","t_us":%lld,"frame":%llu,)"
                 R"("t_input_us":%lld,"t_step_us":%lld,"t_draw_world_us":%lld,)"
                 R"("t_draw_hud_us":%lld,"t_swap_us":%lld,"t_sleep_us":%lld,)"
                 R"("t_total_us":%lld,"fps":%d,"vp_w":%d,"vp_h":%d,)"
                 R"("focused":%s,"minimized":%s,"hidden":%s})"
                 "\n",
                 static_cast<long long>(monotonic_us_since(m_start)),
                 static_cast<unsigned long long>(r.frame),
                 static_cast<long long>(r.t_input_us),
                 static_cast<long long>(r.t_step_us),
                 static_cast<long long>(r.t_draw_world_us),
                 static_cast<long long>(r.t_draw_hud_us),
                 static_cast<long long>(r.t_swap_us),
                 static_cast<long long>(r.t_sleep_us),
                 static_cast<long long>(r.t_total_us),
                 r.fps,
                 r.vp_w,
                 r.vp_h,
                 r.focused ? "true" : "false",
                 r.minimized ? "true" : "false",
                 r.hidden ? "true" : "false");
}

void telemetry::emit_physics(const physics_record& r) noexcept {
    if (m_file == nullptr || m_level < telemetry_level::verbose) {
        return;
    }
    std::fprintf(m_file,
                 R"({"kind":"physics","t_us":%lld,"frame":%llu,)"
                 R"("bodies":%zu,"constraints":%zu,"particles":%zu,)"
                 R"("paused":%s,"scene":"%s","seed":%llu,)"
                 R"("state_digest":"%016llx","substeps":%u,)"
                 R"("sim_time":%.6f,"wall_time":%.6f})"
                 "\n",
                 static_cast<long long>(monotonic_us_since(m_start)),
                 static_cast<unsigned long long>(r.frame),
                 r.bodies,
                 r.constraints,
                 r.particles,
                 r.paused ? "true" : "false",
                 r.scene,
                 static_cast<unsigned long long>(r.seed),
                 static_cast<unsigned long long>(r.state_digest),
                 r.substeps_this_frame,
                 r.sim_time,
                 r.wall_time);
}

void telemetry::emit_render(const render_record& r) noexcept {
    if (m_file == nullptr || m_level < telemetry_level::verbose) {
        return;
    }
    std::fprintf(m_file,
                 R"({"kind":"render","t_us":%lld,"frame":%llu,)"
                 R"("world_calls":%u,"hud_real_refreshes":%u,)"
                 R"("hud_blits":%u,"draw_text_calls":%u})"
                 "\n",
                 static_cast<long long>(monotonic_us_since(m_start)),
                 static_cast<unsigned long long>(r.frame),
                 r.world_calls,
                 r.hud_real_refreshes,
                 r.hud_blits,
                 r.draw_text_calls);
}

void telemetry::emit_event(const char* category,
                           const char* msg,
                           const char* payload_json) noexcept {
    if (m_file == nullptr) {
        return;
    }
    if (payload_json != nullptr && *payload_json != '\0') {
        std::fprintf(m_file,
                     R"({"kind":"event","t_us":%lld,"category":"%s","msg":"%s",%s})"
                     "\n",
                     static_cast<long long>(monotonic_us_since(m_start)),
                     category,
                     msg != nullptr ? msg : "",
                     payload_json);
    } else {
        std::fprintf(m_file,
                     R"({"kind":"event","t_us":%lld,"category":"%s","msg":"%s"})"
                     "\n",
                     static_cast<long long>(monotonic_us_since(m_start)),
                     category,
                     msg != nullptr ? msg : "");
    }
}

void telemetry::maybe_stderr_tick(std::uint64_t frame,
                                  double fps,
                                  double p50_ms,
                                  double p95_ms) noexcept {
    // ~ once per second at 60 Hz.
    if (frame < m_last_stderr_tick_frame + 60u) {
        return;
    }
    m_last_stderr_tick_frame = frame;
    const double t_s = static_cast<double>(monotonic_us_since(m_start)) / 1.0e6;
    std::fprintf(stderr,
                 "[T+%6.1fs] f=%llu fps=%.1f p50=%.2fms p95=%.2fms hb=%llu\n",
                 t_s,
                 static_cast<unsigned long long>(frame),
                 fps,
                 p50_ms,
                 p95_ms,
                 static_cast<unsigned long long>(heartbeat_count()));
}

// ---- Crash handlers ----

namespace {

const char* signal_name(int signo) noexcept {
    switch (signo) {
        case SIGSEGV:
            return "SIGSEGV";
#if defined(SIGBUS)
        case SIGBUS:
            return "SIGBUS";
#endif
        case SIGABRT:
            return "SIGABRT";
        case SIGFPE:
            return "SIGFPE";
        case SIGILL:
            return "SIGILL";
        default:
            return "SIG?";
    }
}

extern "C" void crash_signal_handler(int signo) noexcept {
    if (g_crash_in_progress.fetch_add(1, std::memory_order_acq_rel) > 0) {
        // Re-entry. Bail to default disposition.
        std::signal(signo, SIG_DFL);
        std::raise(signo);
        return;
    }

    // Async-signal-safe banner first.
    safe_write(STDERR_FILENO, "\n=== ea-sandbox CRASH: ");
    safe_write(STDERR_FILENO, signal_name(signo));
    safe_write(STDERR_FILENO, " ===\n");

#if defined(__APPLE__) || defined(__linux__)
    void* frames[64];
    const int n = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
#endif

    // Best-effort: emit a JSONL crash record. fprintf is *not*
    // strictly async-signal-safe but is widely safe for this purpose
    // and we are already in a terminal state.
    telemetry& t = telemetry::get();
    if (t.file_enabled()) {
        t.emit_event("crash", signal_name(signo), nullptr);
        t.shutdown();
    }

    // Restore default and re-raise so the OS / debugger gets the truth.
    std::signal(signo, SIG_DFL);
    std::raise(signo);
}

}  // namespace

void install_crash_handlers() noexcept {
    std::signal(SIGSEGV, crash_signal_handler);
#if defined(SIGBUS)
    std::signal(SIGBUS, crash_signal_handler);
#endif
    std::signal(SIGABRT, crash_signal_handler);
    std::signal(SIGFPE, crash_signal_handler);
    std::signal(SIGILL, crash_signal_handler);
}

// ---- Watchdog ----

namespace {

void watchdog_main(int threshold_ms) noexcept {
    telemetry& t = telemetry::get();
    std::uint64_t last = t.heartbeat_count();
    auto last_change = telemetry::clock::now();
    bool reported = false;

    while (!g_watchdog_stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        const std::uint64_t cur = t.heartbeat_count();
        if (cur != last) {
            last = cur;
            last_change = telemetry::clock::now();
            if (reported) {
                t.emit_event("watchdog", "recovered", nullptr);
                std::fprintf(stderr, "[watchdog] heartbeat recovered after stall\n");
                reported = false;
            }
        } else {
            const auto stalled = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     telemetry::clock::now() - last_change)
                                     .count();
            if (stalled > threshold_ms && !reported) {
                char payload[128];
                std::snprintf(payload,
                              sizeof(payload),
                              R"("stalled_ms":%lld,"heartbeat":%llu)",
                              static_cast<long long>(stalled),
                              static_cast<unsigned long long>(cur));
                t.emit_event("watchdog", "hang", payload);
                std::fprintf(stderr,
                             "[watchdog] HANG: heartbeat stuck %lld ms (count=%llu)\n",
                             static_cast<long long>(stalled),
                             static_cast<unsigned long long>(cur));
                reported = true;
            }
        }
    }
    g_watchdog_running.store(false, std::memory_order_release);
}

}  // namespace

void start_watchdog(int threshold_ms) noexcept {
    bool expected = false;
    if (!g_watchdog_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    g_watchdog_stop.store(false, std::memory_order_release);
    g_watchdog_threshold_ms = threshold_ms;
    std::thread(watchdog_main, threshold_ms).detach();
}

void stop_watchdog() noexcept {
    if (!g_watchdog_running.load(std::memory_order_acquire)) {
        return;
    }
    g_watchdog_stop.store(true, std::memory_order_release);
    // Detached thread will exit on next poll; we do not join.
}

}  // namespace ea_sandbox
