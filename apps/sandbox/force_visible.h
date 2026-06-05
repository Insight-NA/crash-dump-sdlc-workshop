/*
 * apps/sandbox/force_visible.h — C/C++-callable declaration for the
 * Obj-C nuclear window-visibility helper.
 *
 * Mirrors apps/poc/force_visible.h; same Cocoa strategy applied to
 * ea-sandbox.  See force_visible.m for implementation details.
 */

#pragma once

#ifdef __APPLE__
#ifdef __cplusplus
extern "C" {
#endif

void sandbox_force_visible(void);

#ifdef __cplusplus
}
#endif
#endif /* __APPLE__ */
