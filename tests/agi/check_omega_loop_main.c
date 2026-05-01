/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * Ω-loop harness (eight checks + embedded self-test).
 */
#include "../../src/sigma/omega_loop.h"
#include "continual_learning.h"
#include "evolver.h"
#include "gvu_loop.h"
#include "predictive_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct cos_omega_config c;
    struct cos_omega_state  s;
    char                   *rep;

    if (cos_omega_init(NULL, NULL) != -1)
        return 10;

    memset(&c, 0, sizeof c);
    c.max_turns           = 2;
    c.simulation_mode     = 1;
    c.enable_consciousness = 0;
    c.enable_federation   = 0;

    if (cos_omega_init(&c, &s) != 0)
        return 11;

    if (s.running != 1)
        return 12;

    if (cos_omega_step(NULL) != -1)
        return 13;

    if (cos_omega_step(&s) != 0)
        return 14;

    if (cos_omega_halt(&s, "harness") != 0)
        return 15;

    if (!s.halt_requested)
        return 16;

    rep = cos_omega_report(&s);
    if (!rep || strstr(rep, "Ω-Loop") == NULL) {
        free(rep);
        return 17;
    }
    free(rep);

    if (cos_evolver_self_test() != 0)
        return 19;
    if (cos_gvu_self_test() != 0)
        return 20;
    if (cos_predictive_world_self_test() != 0)
        return 21;
    if (cos_continual_learning_self_test() != 0)
        return 22;
    return cos_omega_self_test() != 0 ? 18 : 0;
}
