/*
 * v297 σ-Clock — time-invariant code.
 *
 *   The Giza pyramid does not know what year it is.
 *   It just stands.  Code that does not depend on
 *   absolute time lasts longer: no expiry field, no
 *   wallclock in the hot path, no epoch-dependent
 *   log, no protocol version stamped on the struct.
 *   v297 types four v0 manifests for "the kernel
 *   works the same in 2026 and 2126".
 *
 *   v0 manifests (strict, enumerated):
 *
 *   No expiry (exactly 3 canonical forbidden fields):
 *     `hardcoded_date` · `valid_until_2030` ·
 *     `api_version_expiry`, each
 *     `present_in_kernel = false` AND
 *     `forbidden == true`.
 *     Contract: 3 rows in order; every row forbidden
 *     AND absent — the kernel never reads a calendar.
 *
 *   Monotonic-time only (exactly 3 canonical time
 *   sources):
 *     `CLOCK_MONOTONIC` ALLOW  (in_kernel=true);
 *     `CLOCK_REALTIME`  FORBID (in_kernel=false);
 *     `wallclock_local` FORBID (in_kernel=false).
 *     Contract: 3 rows in order; exactly 1 ALLOW
 *     (monotonic) AND exactly 2 FORBID (wallclock
 *     sources) — the kernel only asks "has more time
 *     passed than X?", never "what time is it?".
 *
 *   Epoch-free logging (exactly 3 canonical logging
 *   properties):
 *     `relative_sequence`    holds=true;
 *     `unix_epoch_absent`    holds=true;
 *     `y2038_safe`           holds=true.
 *     Contract: 3 rows in order; every row `holds =
 *     true` — σ-log stores causal order "A before B",
 *     not Unix seconds-since-1970.
 *
 *   Version-free protocol (exactly 3 canonical
 *   forward-compat properties):
 *     `no_version_field_on_struct`       holds=true;
 *     `old_reader_ignores_new_fields`    holds=true;
 *     `append_only_field_semantics`      holds=true.
 *     Contract: 3 rows in order; every row `holds =
 *     true` — same `sigma_measurement_t` struct works
 *     in v1 and v1000; new fields append, old
 *     readers ignore them.
 *
 *   σ_clock (surface hygiene):
 *       σ_clock = 1 −
 *         (expiry_rows_ok +
 *          expiry_all_forbidden_ok +
 *          expiry_all_absent_ok +
 *          time_rows_ok + time_allow_count_ok +
 *          time_forbid_count_ok +
 *          log_rows_ok + log_all_hold_ok +
 *          proto_rows_ok + proto_all_hold_ok) /
 *         (3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 3 + 1)
 *   v0 requires `σ_clock == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 expiry rows; all forbidden AND all absent.
 *     2. 3 time sources; exactly 1 ALLOW
 *        (CLOCK_MONOTONIC); exactly 2 FORBID.
 *     3. 3 logging properties; all hold.
 *     4. 3 protocol-compat properties; all hold.
 *     5. σ_clock ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v297.1 (named, not implemented): a `cos audit
 *     --time` pass scanning source for
 *     `CLOCK_REALTIME`, hard-coded years, and
 *     `struct { int version; ... }` on the wire,
 *     and a CI job that runs the whole gate matrix
 *     with the host clock set to 2126 to prove the
 *     kernel does not read the calendar.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V297_CLOCK_H
#define COS_V297_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V297_N_EXPIRY 3
#define COS_V297_N_TIME   3
#define COS_V297_N_LOG    3
#define COS_V297_N_PROTO  3

typedef enum {
    COS_V297_TIME_ALLOW  = 1,
    COS_V297_TIME_FORBID = 2
} cos_v297_time_verdict_t;

typedef struct {
    char  field[22];
    bool  present_in_kernel;
    bool  forbidden;
} cos_v297_expiry_t;

typedef struct {
    char                      source[20];
    cos_v297_time_verdict_t   verdict;
    bool                      in_kernel;
} cos_v297_time_t;

typedef struct {
    char  property[22];
    bool  holds;
} cos_v297_log_t;

typedef struct {
    char  property[32];
    bool  holds;
} cos_v297_proto_t;

typedef struct {
    cos_v297_expiry_t  expiry[COS_V297_N_EXPIRY];
    cos_v297_time_t    time  [COS_V297_N_TIME];
    cos_v297_log_t     log   [COS_V297_N_LOG];
    cos_v297_proto_t   proto [COS_V297_N_PROTO];

    int   n_expiry_rows_ok;
    bool  expiry_all_forbidden_ok;
    bool  expiry_all_absent_ok;

    int   n_time_rows_ok;
    bool  time_allow_count_ok;
    bool  time_forbid_count_ok;

    int   n_log_rows_ok;
    bool  log_all_hold_ok;

    int   n_proto_rows_ok;
    bool  proto_all_hold_ok;

    float sigma_clock;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v297_state_t;

void   cos_v297_init(cos_v297_state_t *s, uint64_t seed);
void   cos_v297_run (cos_v297_state_t *s);

size_t cos_v297_to_json(const cos_v297_state_t *s,
                         char *buf, size_t cap);

int    cos_v297_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V297_CLOCK_H */
