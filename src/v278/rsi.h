/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/* v278 σ-Recursive-Self-Improve — manifest (docs/SURFACE_VERSIONS.md). */
#ifndef COS_V278_RSI_H
#define COS_V278_RSI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V278_N_CALIB 4
#define COS_V278_N_ARCH 3
#define COS_V278_N_TAU 3
#define COS_V278_N_GOEDEL 3
#define COS_V278_DENOMINATOR 21

typedef enum {
    COS_V278_SELF_CONFIDENT    = 1,
    COS_V278_CALL_PROCONDUCTOR = 2
} cos_v278_godel_action_t;

typedef struct {
    int   n_channels;
    float aurcc;
    bool  chosen;
} cos_v278_arch_row_t;

typedef struct {
    char  domain[16];
    float tau;
} cos_v278_tau_row_t;

typedef struct {
    char                    tag[12];
    float                   sigma_goedel;
    cos_v278_godel_action_t action;
} cos_v278_godel_row_t;

typedef struct {
    float calibration_err[COS_V278_N_CALIB];

    cos_v278_arch_row_t arch[COS_V278_N_ARCH];
    cos_v278_tau_row_t  tau[COS_V278_N_TAU];
    cos_v278_godel_row_t goedel[COS_V278_N_GOEDEL];

    float tau_goedel;

    /* Calibration (weight 4+1+1) */
    bool cal_cmp_01;
    bool cal_cmp_12;
    bool cal_cmp_23;
    bool cal_last_ok;
    bool cal_span_ok;
    bool cal_range_ok;

    /* Arch (3+1+1) */
    int  n_arch_row_ok;
    bool arch_argmax_ok;
    bool arch_aurcc_diverse;

    /* Thresholds (3+1+1) */
    int  n_tau_row_ok;
    bool tau_open_interval;
    bool tau_distinct_ok;

    /* Gödel (3+1+1) */
    int  n_godel_row_ok;
    bool godel_both_ok;
    bool godel_tau_ok;

    int      passing;
    float    sigma_rsi;
    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v278_state_t;

void   cos_v278_init(cos_v278_state_t *s, uint64_t seed);
void   cos_v278_run(cos_v278_state_t *s);
size_t cos_v278_to_json(const cos_v278_state_t *s, char *buf, size_t cap);
int    cos_v278_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V278_RSI_H */
