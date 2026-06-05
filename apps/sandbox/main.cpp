// ea-sandbox — entry point.
//
// Two run modes:
//   1. Interactive (default): opens a raylib window and renders the scene.
//   2. Headless: --headless --seed S --frames N [--out trace.csv]
//      Used by CI to assert deterministic traces (canonical branch) or to
//      demonstrate non-determinism (seeded-bug branch, BUG-004).
//
// Constitutional articles:
//   - 1 (no exceptions): argv parsing returns a status enum, no throw.
//   - 2 (no RTTI): no dynamic_cast, no typeid.
//   - 3 (EASTL): eastl::string_view used for argv scanning.
//   - 5 (determinism): seeded engine_demo::sim::rng feeds scene init.

#include <EASTL/string_view.h>

#include "app.h"
#include "headless.h"
#include "scene.h"
#include "telemetry.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <timeapi.h>
#endif

namespace {

#if defined(__APPLE__) || defined(__linux__)
[[nodiscard]] inline int getpid_compat() noexcept {
    return static_cast<int>(::getpid());
}
#else
[[nodiscard]] inline int getpid_compat() noexcept {
    return 0;
}
#endif

enum class run_mode : std::uint8_t {
    interactive,
    headless,
};

struct cli_options {
    run_mode mode{run_mode::interactive};
    std::uint64_t seed{42};
    std::uint32_t frames{600};
    const char* out_path{"trace.csv"};
    const char* screenshot_path{nullptr};
    int screenshot_warmup_frames{120};
    ea_sandbox::scene_kind scene{ea_sandbox::scene_kind::rope};
    // Telemetry.
    bool debug{false};  // shorthand for --telemetry-level=frame
    ea_sandbox::telemetry_level telemetry_level{ea_sandbox::telemetry_level::off};
    bool telemetry_level_explicit{false};  // user passed --telemetry-level
    const char* telemetry_out{nullptr};
    bool watchdog{false};           // start hang-watchdog jthread
    bool no_crash_handlers{false};  // skip signal install (for debugger)
};

[[nodiscard]] bool parse_telemetry_level(const char* s, ea_sandbox::telemetry_level& out) noexcept {
    if (s == nullptr)
        return false;
    const eastl::string_view v{s};
    if (v == "off") {
        out = ea_sandbox::telemetry_level::off;
        return true;
    }
    if (v == "frame") {
        out = ea_sandbox::telemetry_level::frame;
        return true;
    }
    if (v == "verbose") {
        out = ea_sandbox::telemetry_level::verbose;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_scene(const char* s, ea_sandbox::scene_kind& out) noexcept {
    if (s == nullptr)
        return false;
    const eastl::string_view v{s};
    if (v == "rope") {
        out = ea_sandbox::scene_kind::rope;
        return true;
    }
    if (v == "pendulum" || v == "pendulum_tower") {
        out = ea_sandbox::scene_kind::pendulum_tower;
        return true;
    }
    if (v == "cloth") {
        out = ea_sandbox::scene_kind::cloth;
        return true;
    }
    if (v == "storm" || v == "particle_storm") {
        out = ea_sandbox::scene_kind::particle_storm;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_uint64(const char* s, std::uint64_t& out) noexcept {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    std::uint64_t v = 0;
    for (const char* p = s; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        v = v * 10u + static_cast<std::uint64_t>(*p - '0');
    }
    out = v;
    return true;
}

[[nodiscard]] bool parse_uint32(const char* s, std::uint32_t& out) noexcept {
    std::uint64_t v = 0;
    if (!parse_uint64(s, v) || v > 0xFFFFFFFFull) {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

void print_usage() noexcept {
    std::fputs(
        "Usage: ea-sandbox [--headless --seed N --frames N --out PATH]\n"
        "                  [--scene rope|pendulum|cloth|storm]\n"
        "                  [--screenshot PATH [--warmup N]]\n"
        "\n"
        "Interactive (default): opens a window. Controls:\n"
        "  1 / 2 / 3 / 4   switch scene (rope / pendulum / cloth / storm)\n"
        "  Space           pause / resume\n"
        "  S               single-step (when paused)\n"
        "  R               reseed scene (visualizes BUG-004 non-determinism)\n"
        "  H               toggle HUD\n"
        "  T               toggle particle trails\n"
        "  + / -           simulation speed up / down\n"
        "  LMB drag        grab + drag a rope/cloth node\n"
        "  RMB             spawn a particle burst at the cursor\n"
        "  F1              deliberate crash (Session 02 demo)\n"
        "  Esc             quit\n"
        "\n"
        "Telemetry (interactive only):\n"
        "  --debug                       enable frame-level telemetry to stderr\n"
        "  --telemetry-level LVL         off | frame | verbose\n"
        "  --telemetry-out PATH          write JSONL records to PATH\n"
        "  --watchdog                    start hang-detection thread (debug only)\n"
        "  --no-crash-handlers           skip SIGSEGV/SIGBUS/etc handler install\n"
        "\n"
        "Headless: writes one CSV row per simulated frame and prints a trace digest.\n",
        stderr);
}

[[nodiscard]] int parse_argv(int argc, char** argv, cli_options& opts) noexcept {
    for (int i = 1; i < argc; ++i) {
        const eastl::string_view arg{argv[i]};
        if (arg == "--headless") {
            opts.mode = run_mode::headless;
        } else if (arg == "--seed" && i + 1 < argc) {
            if (!parse_uint64(argv[++i], opts.seed)) {
                std::fprintf(stderr, "ea-sandbox: invalid --seed value\n");
                return 2;
            }
        } else if (arg == "--frames" && i + 1 < argc) {
            if (!parse_uint32(argv[++i], opts.frames)) {
                std::fprintf(stderr, "ea-sandbox: invalid --frames value\n");
                return 2;
            }
        } else if (arg == "--out" && i + 1 < argc) {
            opts.out_path = argv[++i];
        } else if (arg == "--screenshot" && i + 1 < argc) {
            opts.screenshot_path = argv[++i];
        } else if (arg == "--warmup" && i + 1 < argc) {
            std::uint32_t w = 0;
            if (!parse_uint32(argv[++i], w) || w == 0 || w > 100000u) {
                std::fprintf(stderr, "ea-sandbox: invalid --warmup value\n");
                return 2;
            }
            opts.screenshot_warmup_frames = static_cast<int>(w);
        } else if (arg == "--scene" && i + 1 < argc) {
            if (!parse_scene(argv[++i], opts.scene)) {
                std::fprintf(stderr,
                             "ea-sandbox: invalid --scene value (rope|pendulum|cloth|storm)\n");
                return 2;
            }
        } else if (arg == "--debug") {
            opts.debug = true;
        } else if (arg == "--telemetry-level" && i + 1 < argc) {
            if (!parse_telemetry_level(argv[++i], opts.telemetry_level)) {
                std::fprintf(stderr, "ea-sandbox: invalid --telemetry-level (off|frame|verbose)\n");
                return 2;
            }
            opts.telemetry_level_explicit = true;
        } else if (arg == "--telemetry-out" && i + 1 < argc) {
            opts.telemetry_out = argv[++i];
        } else if (arg == "--watchdog") {
            opts.watchdog = true;
        } else if (arg == "--no-crash-handlers") {
            opts.no_crash_handlers = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 1;
        } else {
            std::fprintf(stderr, "ea-sandbox: unknown argument: %s\n", argv[i]);
            print_usage();
            return 2;
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    cli_options opts{};
    const int parse_rc = parse_argv(argc, argv, opts);
    if (parse_rc == 1) {
        return 0;  // --help
    }
    if (parse_rc != 0) {
        return parse_rc;
    }

    if (opts.mode == run_mode::headless) {
        const ea_sandbox::headless_result r =
            ea_sandbox::run_headless(opts.seed, opts.frames, opts.out_path, opts.scene);
        if (r.status != ea_sandbox::headless_status::ok) {
            std::fprintf(stderr,
                         "ea-sandbox: headless run failed (status=%u)\n",
                         static_cast<unsigned>(r.status));
            return 3;
        }
        std::fprintf(stdout,
                     "trace_digest=%016llx frames=%u scene=%s\n",
                     static_cast<unsigned long long>(r.digest),
                     r.frames_written,
                     ea_sandbox::scene_kind_name(opts.scene));
        return 0;
    }

    // Resolve telemetry level: explicit flag wins; --debug sets frame.
    ea_sandbox::telemetry_level resolved_level = opts.telemetry_level;
    if (!opts.telemetry_level_explicit && opts.debug) {
        resolved_level = ea_sandbox::telemetry_level::frame;
    }

    ea_sandbox::telemetry_options topts{};
    topts.level = resolved_level;
    topts.out_path = opts.telemetry_out;
    topts.watchdog = opts.watchdog;
    ea_sandbox::telemetry::get().init(topts);

    if (!opts.no_crash_handlers) {
        ea_sandbox::install_crash_handlers();
    }
    if (opts.watchdog) {
        ea_sandbox::start_watchdog(2000);
    }

    // One-shot boot record: pid, argv[0], cwd are useful when triaging
    // hangs against a JSONL log without other context.
    {
        char payload[512];
        std::snprintf(payload,
                      sizeof(payload),
                      R"("pid":%d,"argv0":"%s","seed":%llu,"scene":"%s")",
                      static_cast<int>(getpid_compat()),
                      argv[0],
                      static_cast<unsigned long long>(opts.seed),
                      ea_sandbox::scene_kind_name(opts.scene));
        ea_sandbox::telemetry::get().emit_event("boot", "interactive", payload);
    }

    // On Windows the default multimedia timer resolution is ~15.6ms.
    // std::this_thread::sleep_for() in the frame limiter uses this timer,
    // so every 2ms sleep actually sleeps 15ms+ — the loop runs at ~32ms/frame
    // (30fps) and Windows marks the window as "Not Responding".
    // timeBeginPeriod(1) requests 1ms resolution for the life of the process.
#if defined(_WIN32)
    timeBeginPeriod(1);
#endif

    const int rc = ea_sandbox::run_interactive(
        opts.seed, opts.scene, opts.screenshot_path, opts.screenshot_warmup_frames);

#if defined(_WIN32)
    timeEndPeriod(1);
#endif

    ea_sandbox::stop_watchdog();
    ea_sandbox::telemetry::get().shutdown();
    return rc;
}
