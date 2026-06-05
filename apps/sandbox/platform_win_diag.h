// platform_win_diag.h — Windows-only GL/DWM/HWND diagnostic helpers.
// All functions are no-ops on non-Windows builds.
#pragma once

namespace ea_sandbox {

#if defined(_WIN32)

void platform_win_pre_init() noexcept;
void platform_win_post_init() noexcept;
void platform_win_diag_startup() noexcept;
void platform_win_diag_frame(int frame) noexcept;
void platform_win_gl_finish() noexcept;
void platform_win_force_present() noexcept;
void platform_win_setup() noexcept;
void platform_win_gdi_present() noexcept;

#else

inline void platform_win_pre_init() noexcept {}
inline void platform_win_post_init() noexcept {}
inline void platform_win_diag_startup() noexcept {}
inline void platform_win_diag_frame(int) noexcept {}
inline void platform_win_gl_finish() noexcept {}
inline void platform_win_force_present() noexcept {}
inline void platform_win_setup() noexcept {}
inline void platform_win_gdi_present() noexcept {}

#endif  // _WIN32

}  // namespace ea_sandbox
