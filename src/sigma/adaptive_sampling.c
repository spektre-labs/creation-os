/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "adaptive_sampling.h"

#include <stdlib.h>
#include <string.h>

int cos_semantic_adaptive_enabled(void)
{
    const char *e = getenv("COS_SEMANTIC_SIGMA_ADAPTIVE");
    return (e != NULL && e[0] == '1' && e[1] == '\0') ? 1 : 0;
}

static float env_or(const char *name, float def)
{
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0')
        return def;
    float x = (float)strtod(v, NULL);
    return x;
}

float cos_adaptive_sigma_env_low(void)
{
    return env_or("COS_ADAPTIVE_SIGMA_LOW", 0.15f);
}

float cos_adaptive_sigma_env_high(void)
{
    return env_or("COS_ADAPTIVE_SIGMA_HIGH", 0.75f);
}

int cos_adaptive_take_third_sample(float pair_sigma)
{
    float lo = cos_adaptive_sigma_env_low();
    float hi = cos_adaptive_sigma_env_high();
    if (pair_sigma <= lo || pair_sigma >= hi)
        return 0;
    return 1;
}
