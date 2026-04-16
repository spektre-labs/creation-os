/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sandbox.h"

#include <stdlib.h>
#include <string.h>

sandbox_decision_t sandbox_check(const sandbox_config_t *cfg, const char *proposed_action, sigma_decomposed_t sigma)
{
    sandbox_decision_t d = {0};
    if (!cfg) {
        d.allowed = 0;
        d.reason = strdup("missing sandbox config");
        return d;
    }
    const char *act = proposed_action ? proposed_action : "";
    d.proposed_action = strdup(act);
    if (!d.proposed_action) {
        d.allowed = 0;
        d.reason = strdup("allocation failed");
        return d;
    }
    d.action_sigma = sigma.epistemic;

    if (sigma.epistemic > cfg->sigma_threshold_for_action) {
        d.allowed = 0;
        d.reason = strdup("sigma too high for privileged action — model is uncertain");
        return d;
    }

    (void)cfg->require_human_approval_above;
    d.allowed = 1;
    d.reason = strdup("sigma within bounds, action permitted");
    return d;
}

void sandbox_decision_free(sandbox_decision_t *d)
{
    if (!d) {
        return;
    }
    free(d->proposed_action);
    free(d->reason);
    d->proposed_action = NULL;
    d->reason = NULL;
}
