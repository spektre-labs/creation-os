/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "../../src/sigma/sovereign_limits.h"

#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_sovereign_limits lim;
    cos_sovereign_default_limits(&lim);
    lim.require_human_above = 1.0f;
    lim.max_autonomy_level   = 1.0f;
    if (cos_sovereign_init(&lim) != 0)
        return 1;

    struct cos_sovereign_state st;
    memset(&st, 0, sizeof(st));
    if (cos_sovereign_check(&st, COS_SOVEREIGN_ACTION_NETWORK) != 0)
        return 2;

    struct cos_state_ledger L;
    cos_state_ledger_init(&L);
    L.k_eff              = 0.95f;
    L.sigma_mean_session = 0.08f;
    float au             = cos_sovereign_assess_autonomy(&L);
    if (au < 0.f || au > 1.f)
        return 3;

    if (cos_sovereign_self_test() != 0)
        return 4;

    cos_sovereign_brake(&st, "test brake");
    if (!st.human_required)
        return 5;

    cos_sovereign_snapshot_state(&st);
    return 0;
}
