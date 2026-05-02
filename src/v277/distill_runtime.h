/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v277 σ-Distill-Runtime — offline teacher/student + σ-filter manifest (see docs/SURFACE_VERSIONS.md).
 */
#ifndef COS_V277_DISTILL_RUNTIME_H
#define COS_V277_DISTILL_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V277_N_LEARN 4
#define COS_V277_N_DOMAIN 3
#define COS_V277_N_TRAJ 4
#define COS_V277_DENOMINATOR 19

typedef enum {
    COS_V277_LEARN = 1,
    COS_V277_SKIP  = 2
} cos_v277_learn_dec_t;

typedef enum {
    COS_V277_ROUTE_LOCAL = 1,
    COS_V277_ROUTE_API   = 2
} cos_v277_route_t;

typedef struct {
    char               tag[16];
    float              sigma_teacher;
    cos_v277_learn_dec_t decision;
} cos_v277_learn_row_t;

typedef struct {
    char            domain[16];
    float           sigma_domain;
    cos_v277_route_t route;
} cos_v277_domain_row_t;

typedef struct {
    char   month[16];
    float  api_share;
    float  local_share;
    float  cost;
} cos_v277_traj_row_t;

typedef struct {
    char teacher[24];
    char student[24];
    float tau_learn;
    float tau_domain;

    cos_v277_learn_row_t  learn[COS_V277_N_LEARN];
    cos_v277_domain_row_t domain[COS_V277_N_DOMAIN];
    cos_v277_traj_row_t   traj[COS_V277_N_TRAJ];

    int   pair_ok;
    int   n_learn_row_ok;
    bool  learn_both_ok;

    int   n_domain_row_ok;
    bool  domain_both_ok;

    int   n_traj_row_ok;
    bool  traj_api_dec;
    bool  traj_local_inc;
    bool  traj_cost_dec;
    bool  traj_anchor_api_high;
    bool  traj_anchor_api_low;

    int   passing;
    float sigma_distill;
    bool  chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v277_state_t;

void   cos_v277_init(cos_v277_state_t *s, uint64_t seed);
void   cos_v277_run(cos_v277_state_t *s);
size_t cos_v277_to_json(const cos_v277_state_t *s, char *buf, size_t cap);
int    cos_v277_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V277_DISTILL_RUNTIME_H */
