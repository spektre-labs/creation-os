/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v244 σ-Package — cross-platform install artefact
 *   manifest with minimal / full profiles, first-run
 *   experience, and σ-audited update / rollback.
 *
 *   243 kernels in source; v244 turns them into a
 *   one-command install on every major desktop /
 *   server platform.
 *
 *   Platforms (v0 manifest, exactly 4):
 *     macOS   → brew install creation-os
 *     linux   → apt install creation-os
 *     docker  → docker run spektre/creation-os
 *     windows → winget install creation-os
 *
 *   Install profiles (v0):
 *     MINIMAL — the seed quintet {v29, v101, v106,
 *               v124, v148}; ~4 GB RAM, ~2 GB disk;
 *               runs on M1 Air, Raspberry Pi 5.
 *     FULL    — every kernel in the stack
 *               (n_kernels ≥ 243); ~16 GB RAM
 *               recommended, ~15 GB disk; M3/M4 ANE
 *               + AMX dispatch hints live.
 *
 *   First-run sequence (v0, exactly 4 states):
 *     `cos start` →
 *       SEED     ("Presence: SEED. Growing...")
 *       GROWING  (v229 seed → cos_v243_init scaffold)
 *       CHECKING (v243 completeness check)
 *       READY    ("Ready. K_eff: 0.74. Kernels: N active.")
 *
 *   Update / rollback:
 *     A v0 fixture runs 4 updates against a starting
 *     version `v1.0.0`, keeping a two-deep history so
 *     one rollback step restores the prior version
 *     byte-for-byte.  σ_update is the per-update risk
 *     score; any update whose σ exceeds `τ_update =
 *     0.50` is rejected and the installed version is
 *     left untouched.
 *
 *   σ_package (install-path hygiene):
 *       σ_package = 1 − (platforms_healthy /
 *                        n_platforms)
 *     v0 requires σ_package == 0.0 (every platform's
 *     manifest is well-formed).
 *
 *   Contracts (v0):
 *     1. n_platforms == 4, names {macOS, linux,
 *        docker, windows}; every platform carries a
 *        non-empty install command.
 *     2. Minimal profile covers exactly the seed
 *        quintet {29, 101, 106, 124, 148}.
 *     3. Full profile carries n_kernels ≥ 243.
 *     4. First-run visits the four states in strict
 *        order: SEED → GROWING → CHECKING → READY.
 *     5. Update fixture: at least 3 accepted updates;
 *        at least 1 update rejected by σ-gate.
 *     6. rollback_ok == true — after a rollback the
 *        installed version equals the previous version
 *        byte-for-byte.
 *     7. σ_package ∈ [0, 1] AND σ_package == 0.0.
 *     8. FNV-1a chain replays byte-identically.
 *
 *   v244.1 (named, not implemented): live Homebrew
 *     tap, Debian / RPM / Nix packaging, signed Docker
 *     manifests, Windows Store metadata, real
 *     mmap-based incremental update via v107 installer.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V244_PACKAGE_H
#define COS_V244_PACKAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V244_N_PLATFORMS      4
#define COS_V244_SEED_QUINTET     5
#define COS_V244_N_FIRSTRUN       4
#define COS_V244_N_UPDATES        4
#define COS_V244_MIN_FULL_KERNELS 243

typedef enum {
    COS_V244_PROFILE_MINIMAL = 1,
    COS_V244_PROFILE_FULL    = 2
} cos_v244_profile_t;

typedef enum {
    COS_V244_STATE_SEED     = 1,
    COS_V244_STATE_GROWING  = 2,
    COS_V244_STATE_CHECKING = 3,
    COS_V244_STATE_READY    = 4
} cos_v244_state_t;

typedef struct {
    char  name[16];        /* macOS | linux | docker | windows */
    char  install_cmd[64];
    bool  manifest_ok;
} cos_v244_platform_t;

typedef struct {
    cos_v244_profile_t profile;
    int                kernels[COS_V244_MIN_FULL_KERNELS + 8];
    int                n_kernels;
    int                ram_mb;
    int                disk_mb;
} cos_v244_install_t;

typedef struct {
    int                 step;
    cos_v244_state_t    state;
    char                banner[48];
    int                 tick;
} cos_v244_firstrun_t;

typedef struct {
    int    step;
    char   from_version[12];
    char   to_version[12];
    float  sigma_update;
    bool   accepted;
} cos_v244_update_t;

typedef struct {
    cos_v244_platform_t platforms[COS_V244_N_PLATFORMS];
    int                 n_platforms_ok;

    cos_v244_install_t  minimal;
    cos_v244_install_t  full;

    cos_v244_firstrun_t firstrun[COS_V244_N_FIRSTRUN];

    cos_v244_update_t   updates [COS_V244_N_UPDATES];
    int                 n_updates_accepted;
    int                 n_updates_rejected;
    char                installed_version[12];
    char                previous_version [12];
    bool                rollback_ok;
    float               tau_update;

    float               sigma_package;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v244_state_full_t;

void   cos_v244_init(cos_v244_state_full_t *s, uint64_t seed);
void   cos_v244_run (cos_v244_state_full_t *s);

size_t cos_v244_to_json(const cos_v244_state_full_t *s,
                         char *buf, size_t cap);

int    cos_v244_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V244_PACKAGE_H */
