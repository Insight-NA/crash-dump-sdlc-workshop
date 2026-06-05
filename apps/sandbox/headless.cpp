#include "headless.h"

#include "scene.h"

#include <cstdio>

namespace ea_sandbox {

headless_result run_headless(std::uint64_t seed,
                             std::uint32_t frames,
                             const char* out_path,
                             scene_kind kind) noexcept {
    headless_result r{};
    if (out_path == nullptr || frames == 0) {
        r.status = headless_status::invalid_arguments;
        return r;
    }

    std::FILE* f = std::fopen(out_path, "wb");
    if (f == nullptr) {
        r.status = headless_status::cannot_open_output;
        return r;
    }

    if (std::fprintf(f, "frame,sim_time,digest,rope0_x,rope0_y,ropeN_x,ropeN_y\n") < 0) {
        std::fclose(f);
        r.status = headless_status::write_failed;
        return r;
    }

    scene s{seed};
    if (kind != scene_kind::rope) {
        s.switch_scene(kind);
    }

    for (std::uint32_t i = 0; i < frames; ++i) {
        (void)s.step(scene::kFixedStepSeconds);

        double r0x = 0.0;
        double r0y = 0.0;
        double rNx = 0.0;
        double rNy = 0.0;
        (void)s.rope_node_position(0, r0x, r0y);
        const std::size_t last = s.rope_node_count() > 0 ? s.rope_node_count() - 1 : 0;
        (void)s.rope_node_position(last, rNx, rNy);

        const std::uint64_t d = s.state_digest();
        if (std::fprintf(f,
                         "%u,%.9f,%016llx,%.9f,%.9f,%.9f,%.9f\n",
                         i,
                         s.sim_time_seconds(),
                         static_cast<unsigned long long>(d),
                         r0x,
                         r0y,
                         rNx,
                         rNy) < 0) {
            std::fclose(f);
            r.status = headless_status::write_failed;
            return r;
        }
        ++r.frames_written;
    }

    r.digest = s.state_digest();
    std::fclose(f);
    return r;
}

}  // namespace ea_sandbox
