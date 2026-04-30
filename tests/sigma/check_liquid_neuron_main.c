/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/sigma/liquid_neuron.h"

#include <stdio.h>

int main(void)
{
    liquid_sigma_neuron_t n;
    int i;

    liquid_sigma_neuron_init(&n, COS_LIQUID_Q16 / 4);
    for (i = 0; i < 32; i++)
        (void)liquid_sigma_neuron_update(&n, (int32_t)10000);

    (void)n.state;
    puts("check_liquid_neuron_main: OK");
    return 0;
}
