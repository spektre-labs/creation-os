/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * Green score harness (four checks + embedded self-test).
 */
#include "../../src/sigma/green_score.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct cos_energy_measurement life;
    struct cos_green_score         g;
    char                          *js;

    if (cos_energy_init(NULL) != 0)
        return 21;

    memset(&life, 0, sizeof life);
    life.estimated_joules = 0.08f;
    life.tokens_generated  = 32;
    life.co2_grams         = 0.0015f;

    g = cos_green_calculate(&life);
    if (g.green_score < 0.f || g.green_score > 100.f || g.grade < 'A'
        || g.grade > 'F')
        return 22;

    if (fabsf(g.vs_gpt5_ratio) < 1.f)
        return 23;

    js = cos_green_to_json(&g);
    if (!js || !strstr(js, "\"grade\"")) {
        free(js);
        return 24;
    }
    free(js);

    return cos_green_self_test() != 0 ? 25 : 0;
}
