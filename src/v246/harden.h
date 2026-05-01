/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v246 σ-Harden — production hardening.
 *
 *   Research code → production code.  v0 manifests:
 *
 *   Chaos scenarios (exactly 5, canonical order):
 *     kill-random-kernel
 *     high-load
 *     network-partition
 *     corrupt-memory
 *     oom-attempt
 *   Each scenario declares a recovery path and a
 *   recovery latency in ticks; `recovered == true` is
 *   required for every scenario.
 *
 *   Resource limits (exactly 6, non-negotiable):
 *     max_tokens_per_request       = 16384
 *     max_time_ms_per_request      = 60000
 *     max_kernels_per_request      = 20
 *     max_memory_mb_per_session    = 8192
 *     max_disk_mb_per_session      = 4096
 *     max_concurrent_requests      = 64
 *   Every limit > 0.  v244 σ-budget is the binding
 *   upstream partner for `max_kernels_per_request`.
 *
 *   Input-validation checks (exactly 5, all pass):
 *     max_prompt_length    (<= 1_000_000 bytes)
 *     utf8_encoding_ok
 *     injection_filter_ok  (v210 guardian)
 *     rate_limit_ok
 *     auth_token_required  (v241 surface)
 *
 *   Security items (exactly 5, all present):
 *     tls_required
 *     auth_token_required
 *     audit_log_on         (v181)
 *     containment_on       (v209)
 *     scsl_license_pinned  (§11)
 *
 *   Error handling:
 *     Each of the 5 chaos scenarios includes an
 *     `error_path_taken` flag.  A scenario that
 *     "panics" (i.e. triggered an abort instead of a
 *     typed error path) would set it to false and the
 *     gate would fail — the v0 fixture has every
 *     scenario flipping to `true`, and v239
 *     deactivation is cited as the binding upstream
 *     recovery contract.
 *
 *   σ_harden (posture score):
 *       σ_harden = 1 −
 *         (recovered_scenarios  +
 *          input_checks_passing +
 *          security_items_on    +
 *          limits_positive) /
 *         (5 + 5 + 5 + 6)
 *   v0 requires `σ_harden == 0.0`.
 *
 *   Contracts (v0):
 *     1. n_chaos == 5, names in canonical order, every
 *        `recovered == true`, every
 *        `error_path_taken == true`, every
 *        `recovery_ticks > 0`.
 *     2. n_limits == 6, every limit strictly positive;
 *        names match the canonical manifest verbatim.
 *     3. n_input_checks == 5, every `pass == true`.
 *     4. n_security == 5, every `on == true`.
 *     5. σ_harden ∈ [0, 1] AND σ_harden == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v246.1 (named, not implemented): live kill-switch
 *     injection via v239 runtime, real concurrent-load
 *     harness, v211 formal proof of the σ-gate
 *     invariants, live audit log rotation, SCSL
 *     license signature verification at boot.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V246_HARDEN_H
#define COS_V246_HARDEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V246_N_CHAOS        5
#define COS_V246_N_LIMITS       6
#define COS_V246_N_INPUT_CHECKS 5
#define COS_V246_N_SECURITY     5

typedef struct {
    char  name[24];
    char  recovery[48];
    int   recovery_ticks;
    bool  error_path_taken;
    bool  recovered;
} cos_v246_chaos_t;

typedef struct {
    char  name[40];
    long  value;
} cos_v246_limit_t;

typedef struct {
    char  name[32];
    bool  pass;
} cos_v246_input_t;

typedef struct {
    char  name[32];
    bool  on;
} cos_v246_security_t;

typedef struct {
    cos_v246_chaos_t    chaos   [COS_V246_N_CHAOS];
    cos_v246_limit_t    limits  [COS_V246_N_LIMITS];
    cos_v246_input_t    inputs  [COS_V246_N_INPUT_CHECKS];
    cos_v246_security_t security[COS_V246_N_SECURITY];

    int                 n_recovered;
    int                 n_limits_positive;
    int                 n_inputs_passing;
    int                 n_security_on;
    float               sigma_harden;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v246_state_t;

void   cos_v246_init(cos_v246_state_t *s, uint64_t seed);
void   cos_v246_run (cos_v246_state_t *s);

size_t cos_v246_to_json(const cos_v246_state_t *s,
                         char *buf, size_t cap);

int    cos_v246_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V246_HARDEN_H */
