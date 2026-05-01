/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v165 σ-Edge — IoT + edge-device runtime model.
 *
 * Creation OS was written for an M3 MacBook.  v165 makes it
 * target a *graph* of constrained devices: Raspberry Pi 5,
 * NVIDIA Jetson Orin Nano, Android (Termux / NDK), iOS
 * (planned).  The host binary is `cos-lite`:
 *
 *   cos-lite = v101 bridge + v29 σ-stack + v106 HTTP server
 *
 * Notably absent from cos-lite: v115 memory, v150 swarm,
 * v127 voice.  They're optional add-ons selectable via
 * v162 σ-compose profiles.
 *
 * v165.0 (this file) ships:
 *   - a `target_profile_t` enum + manifest for each supported
 *     target (rpi5 / jetson_orin / android / macbook_m3 / ios).
 *     Each profile declares: arch, total RAM MB, available RAM
 *     after OS, target binary name, default τ scale.
 *   - σ-adaptive τ: τ_edge(ram_mb) = τ_default * (ram_mb / 8192)
 *     clamped to [0.15, 1.0].  The smaller the device, the higher
 *     the τ, the more abstains, the more honest the model stays.
 *   - a deterministic "fit check": given a profile, does a
 *     (binary_mb, weights_mb, kvcache_mb, sigma_overhead_mb)
 *     budget fit inside available RAM?
 *   - cross-compile command matrix (returned as JSON, not
 *     executed): what Makefile target + toolchain triple to
 *     use.
 *   - a smoke simulator that "boots" cos-lite on a profile and
 *     produces a receipt showing τ_edge, fit verdict, and
 *     expected server port.
 *
 * v165.1 (named, not shipped):
 *   - real cross-compilation Makefile targets
 *     (cos-lite-rpi5 / cos-lite-jetson / cos-lite-android)
 *   - QEMU-emulated ARM64 CI harness
 *   - iOS Swift wrapper over v76 σ-Surface
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V165_EDGE_H
#define COS_V165_EDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V165_MAX_NAME    32
#define COS_V165_MAX_MSG     160
#define COS_V165_N_PROFILES   5

typedef enum {
    COS_V165_TARGET_MACBOOK_M3   = 0,
    COS_V165_TARGET_RPI5         = 1,
    COS_V165_TARGET_JETSON_ORIN  = 2,
    COS_V165_TARGET_ANDROID      = 3,
    COS_V165_TARGET_IOS          = 4,   /* v165.1, still listed */
} cos_v165_target_t;

typedef struct {
    cos_v165_target_t target;
    char     name[COS_V165_MAX_NAME];          /* "rpi5" */
    char     arch[COS_V165_MAX_NAME];          /* "arm64" */
    char     triple[COS_V165_MAX_NAME];        /* "aarch64-linux-gnu" */
    int      total_ram_mb;
    int      available_ram_mb;                 /* after OS */
    bool     has_gpu;
    bool     has_camera;
    int      default_port;                     /* cos-lite HTTP port */
    float    tau_scale;                        /* scales τ_default */
    char     make_target[COS_V165_MAX_NAME];   /* "cos-lite-rpi5" */
    bool     supported_in_v0;
} cos_v165_profile_t;

typedef struct {
    int binary_mb;
    int weights_mb;
    int kvcache_mb;
    int sigma_overhead_mb;
} cos_v165_budget_t;

typedef struct {
    cos_v165_profile_t profile;
    float    tau_default;        /* host τ */
    float    tau_edge;            /* scaled τ for this device */
    int      total_used_mb;
    int      headroom_mb;
    bool     fits;
    char     note[COS_V165_MAX_MSG];
} cos_v165_fit_t;

typedef struct {
    cos_v165_target_t target;
    char     cos_lite_target[COS_V165_MAX_NAME];  /* make target name */
    char     triple[COS_V165_MAX_NAME];
    bool     needs_qemu;
} cos_v165_crosscompile_t;

typedef struct {
    cos_v165_target_t target;
    cos_v165_fit_t    fit;
    bool              booted;
    int               server_port;
    char              boot_msg[COS_V165_MAX_MSG];
} cos_v165_boot_receipt_t;

/* Populate the 5 baked target profiles. */
void cos_v165_profiles_init(cos_v165_profile_t out[COS_V165_N_PROFILES]);

/* Lookup. */
const cos_v165_profile_t *cos_v165_profile_get(cos_v165_target_t t);

/* Compute τ_edge = clamp(τ_default * (ram/8192), 0.15, 1.0). */
float cos_v165_tau_edge(float tau_default, int available_ram_mb);

/* Fit check against a RAM budget. */
cos_v165_fit_t cos_v165_fit(const cos_v165_profile_t *p,
                            const cos_v165_budget_t  *b,
                            float tau_default);

/* Cross-compile matrix (name-only; no shell-out). */
cos_v165_crosscompile_t cos_v165_crosscompile(cos_v165_target_t t);

/* Simulated cos-lite boot on a target; does not touch OS. */
cos_v165_boot_receipt_t cos_v165_boot_cos_lite(cos_v165_target_t t,
                                               float tau_default);

/* JSON serializers. */
size_t cos_v165_profiles_to_json(char *buf, size_t cap,
                                 float tau_default,
                                 const cos_v165_budget_t *default_budget);
size_t cos_v165_boot_to_json(const cos_v165_boot_receipt_t *r,
                             char *buf, size_t cap);

const char *cos_v165_target_name(cos_v165_target_t t);

int cos_v165_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V165_EDGE_H */
