/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Host self-test: sensor deviation → σ-gate (no Arduino / AVR).
 */
#include <stdio.h>
#include <stdlib.h>

#include "sigma_sensor.h"

static int fail(const char *msg)
{
    fprintf(stderr, "check_sigma_sensor: %s\n", msg);
    return 1;
}

int main(void)
{
    sigma_sensor_t s;
    sigma_verdict_t v1, v2;

    sigma_sensor_init(&s, 2200, 500);

    v1 = sigma_sensor_step(&s, 2230);
    if (v1 != SIGMA_RETHINK)
        return fail("first calm step should be RETHINK (d_sigma > 0)");

    v2 = sigma_sensor_step(&s, 2230);
    if (v2 != SIGMA_ACCEPT)
        return fail("second identical calm step should be ACCEPT");

    sigma_sensor_init(&s, 2200, 500);
    v1 = sigma_sensor_step(&s, 4500);
    if (v1 != SIGMA_ABSTAIN)
        return fail("extreme reading should ABSTAIN");

    printf("check_sigma_sensor: OK\n");
    return 0;
}
