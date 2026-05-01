/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * Energy accounting harness (six checks + embedded self-test).
 */
#include "../../src/sigma/energy_accounting.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct cos_energy_measurement a, sum;
    cos_energy_timer_t             tm;
    struct cos_energy_measurement  tm_m;

    if (cos_energy_init(NULL) != 0)
        return 11;

    cos_energy_reset_session();
    if (cos_energy_measure_wall(10.f, 15.f, 10).estimated_joules <= 0.f)
        return 12;

    memset(&sum, 0, sizeof sum);
    a = cos_energy_measure_wall(5.f, 5.f, 2);
    cos_energy_accumulate(&sum, &a);
    if (fabsf(sum.estimated_joules - a.estimated_joules) > 1e-3f)
        return 13;

    {
        char *js = cos_energy_to_json(&a);
        if (!js || !strstr(js, "estimated_joules")) {
            free(js);
            return 14;
        }
        free(js);
    }

    cos_energy_timer_start(&tm);
    tm_m = cos_energy_timer_stop_and_measure(&tm, 0);
    if (tm_m.estimated_joules < 0.f)
        return 15;

    {
        char buf[64];
        if (cos_energy_lifetime_report(buf, 0) != -1)
            return 16;
    }

    return cos_energy_self_test() != 0 ? 17 : 0;
}
