/*
 * σ-sovereign brake — self-limiting action budgets (architecture layer).
 *
 * Distinct from pipeline/cos_sigma_sovereign_t (cost accounting).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SOVEREIGN_LIMITS_H
#define COS_SIGMA_SOVEREIGN_LIMITS_H

#include "state_ledger.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_sovereign_limits {
    int   max_actions_per_hour;
    int   max_network_calls_per_hour;
    int   max_file_writes_per_hour;
    int   max_federation_shares_per_day;
    float max_autonomy_level;
    float require_human_above;
};

struct cos_sovereign_state {
    int actions_this_hour;
    int network_calls_this_hour;
    int file_writes_this_hour;
    int federation_shares_today;
    float current_autonomy;
    int   human_required;
    char  last_brake_reason[256];
};

enum cos_sovereign_action_type {
    COS_SOVEREIGN_ACTION_GENERIC          = 0,
    COS_SOVEREIGN_ACTION_NETWORK          = 1,
    COS_SOVEREIGN_ACTION_FILE_WRITE       = 2,
    COS_SOVEREIGN_ACTION_FEDERATION_SHARE = 3,
    COS_SOVEREIGN_ACTION_REPLICATION      = 4,
    COS_SOVEREIGN_ACTION_SELF_MODIFY      = 5,
};

void cos_sovereign_default_limits(struct cos_sovereign_limits *out);

int cos_sovereign_init(const struct cos_sovereign_limits *limits);

int cos_sovereign_check(struct cos_sovereign_state *state,
                        int                          action_type);

int cos_sovereign_brake(struct cos_sovereign_state *state,
                        const char *reason);

float cos_sovereign_assess_autonomy(const struct cos_state_ledger *ledger);

void cos_sovereign_snapshot_state(struct cos_sovereign_state *out);

int cos_sovereign_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SOVEREIGN_LIMITS_H */
