/*
 * v287 σ-Granite — dependency-free, C99-forever,
 *                   platform-agnostic kernel material.
 *
 *   The pyramids lasted 4500 years because of the
 *   granite.  Code lasts decades because of zero
 *   dependencies: every external import is rusting
 *   steel in a masonry wall.  v287 types the
 *   longevity contract as four v0 manifests so that
 *   a build cannot "accidentally" link a package
 *   manager into a kernel translation unit.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Dependency policy (exactly 6 canonical rows):
 *     `libc` · `posix` · `pthreads` (`ALLOW`,
 *     `in_kernel = true` — eternal system surfaces),
 *     `npm` · `pip` · `cargo` (`FORBID`,
 *     `in_kernel = false` — ecosystem package
 *     managers, explicitly rusting steel).
 *     Contract: canonical names in order; 6 distinct;
 *     every ALLOW row has `in_kernel = true` AND
 *     every FORBID row has `in_kernel = false`.
 *
 *   Language standard (exactly 3 canonical rows):
 *     `C89` (1989, `allowed = true`), `C99` (1999,
 *     `allowed = true`), `C++` (1998,
 *     `allowed = false` — too many failure modes for
 *     a kernel).
 *     Contract: canonical order; exactly 2 allowed
 *     AND exactly 1 forbidden.
 *
 *   Platform coverage (exactly 5 canonical targets):
 *     `linux` · `macos` · `bare_metal` · `rtos` ·
 *     `cortex_m`, each `kernel_works == true` AND
 *     `ifdef_only_at_edges == true` — the kernel
 *     core assumes no OS, hardware, or endianness;
 *     `#ifdef` dispatch is confined to NEON / AVX /
 *     ANE edges.
 *     Contract: 5 distinct names; `kernel_works` on
 *     every target; `ifdef_only_at_edges` on every
 *     target.
 *
 *   Vendoring policy (exactly 3 canonical rows):
 *     `vendored_copy` (`allowed_in_kernel = true`),
 *     `external_linkage` (false),
 *     `supply_chain_trust` (false).
 *     Contract: `vendored_copy` allowed AND both
 *     `external_linkage` and `supply_chain_trust`
 *     forbidden — same discipline as hauling every
 *     block of stone on-site before the pyramid
 *     starts rising.
 *
 *   σ_granite (surface hygiene):
 *       σ_granite = 1 −
 *         (dep_rows_ok + dep_allow_polarity_ok +
 *          dep_forbid_polarity_ok + std_rows_ok +
 *          std_count_ok + platform_rows_ok +
 *          platform_works_ok + platform_ifdef_ok +
 *          vendor_rows_ok + vendor_polarity_ok) /
 *         (6 + 1 + 1 + 3 + 1 + 5 + 1 + 1 + 3 + 1)
 *   v0 requires `σ_granite == 0.0`.
 *
 *   Contracts (v0):
 *     1. 6 dependency rows canonical; 3 ALLOW rows
 *        with `in_kernel = true`; 3 FORBID rows with
 *        `in_kernel = false`.
 *     2. 3 language-standard rows canonical;
 *        C89 / C99 allowed AND C++ forbidden.
 *     3. 5 platform rows canonical; `kernel_works`
 *        AND `ifdef_only_at_edges` on every row.
 *     4. 3 vendoring rows canonical;
 *        `vendored_copy` allowed; `external_linkage`
 *        and `supply_chain_trust` forbidden.
 *     5. σ_granite ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v287.1 (named, not implemented): `cos audit
 *     --deps` actually parsing the in-tree
 *     translation units, emitting a live SBOM pinned
 *     to the same 6-row ALLOW/FORBID contract, plus
 *     a boot-time hash-chain certifying that every
 *     vendored copy matches its upstream at the
 *     pinned version.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V287_GRANITE_H
#define COS_V287_GRANITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V287_N_DEP       6
#define COS_V287_N_STD       3
#define COS_V287_N_PLATFORM  5
#define COS_V287_N_VENDOR    3

typedef enum {
    COS_V287_DEP_ALLOW  = 1,
    COS_V287_DEP_FORBID = 2
} cos_v287_verdict_t;

typedef struct {
    char                name[10];
    cos_v287_verdict_t  verdict;
    bool                in_kernel;
} cos_v287_dep_t;

typedef struct {
    char  name[6];
    int   year;
    bool  allowed;
} cos_v287_std_t;

typedef struct {
    char  name[12];
    bool  kernel_works;
    bool  ifdef_only_at_edges;
} cos_v287_platform_t;

typedef struct {
    char  name[22];
    bool  allowed_in_kernel;
} cos_v287_vendor_t;

typedef struct {
    cos_v287_dep_t       dep      [COS_V287_N_DEP];
    cos_v287_std_t       stdlang  [COS_V287_N_STD];
    cos_v287_platform_t  platform [COS_V287_N_PLATFORM];
    cos_v287_vendor_t    vendor   [COS_V287_N_VENDOR];

    int   n_dep_rows_ok;
    bool  dep_allow_polarity_ok;
    bool  dep_forbid_polarity_ok;

    int   n_std_rows_ok;
    bool  std_count_ok;

    int   n_platform_rows_ok;
    bool  platform_works_ok;
    bool  platform_ifdef_ok;

    int   n_vendor_rows_ok;
    bool  vendor_polarity_ok;

    float sigma_granite;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v287_state_t;

void   cos_v287_init(cos_v287_state_t *s, uint64_t seed);
void   cos_v287_run (cos_v287_state_t *s);

size_t cos_v287_to_json(const cos_v287_state_t *s,
                         char *buf, size_t cap);

int    cos_v287_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V287_GRANITE_H */
