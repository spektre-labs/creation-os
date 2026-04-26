/* BitNet native path — scaffold until a C++ adapter is linked.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "bitnet_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cos_bitnet_native_top2_gap_fraction(int32_t top1, int32_t top2,
                                        float *out_sigma01)
{
    int64_t t1, gap, num, sigma_q16;

    if (out_sigma01 == NULL)
        return -1;
    t1 = (int64_t)top1;
    if (t1 == 0)
        return -1;
    gap = t1 - (int64_t)top2;
    num = (1LL << 16) - ((gap << 16) / t1);
    if (num < 0)
        num = 0;
    if (num > (1LL << 16))
        num = (1LL << 16);
    sigma_q16 = num;
    *out_sigma01 = (float)sigma_q16 / (float)(1 << 16);
    if (*out_sigma01 < 0.f)
        *out_sigma01 = 0.f;
    if (*out_sigma01 > 1.f)
        *out_sigma01 = 1.f;
    return 0;
}

int cos_bitnet_native_try_complete(const char *prompt,
                                   const char *system_prompt, int round,
                                   cos_bitnet_server_result_t *out)
{
#ifndef CREATION_OS_BITNET_NATIVE
    (void)prompt;
    (void)system_prompt;
    (void)round;
    (void)out;
    return -1;
#else
    static int warned;
    const char *e = getenv("COS_BITNET_NATIVE");

    (void)prompt;
    (void)system_prompt;
    (void)round;
    if (out == NULL)
        return -1;
    if (e == NULL || e[0] != '1')
        return -1;
    if (!warned) {
        fprintf(stderr,
                "[bitnet_native] COS_BITNET_NATIVE=1 set, but this binary has "
                "no linked BitNet inference core yet — use llama-server HTTP "
                "(COS_BITNET_SERVER=1) or remove COS_BITNET_NATIVE.\n");
        warned = 1;
    }
    return -1;
#endif
}

int cos_bitnet_native_self_test(void)
{
    float s;
    if (cos_bitnet_native_top2_gap_fraction(1000, 800, &s) != 0)
        return 1;
    if (s < 0.79f || s > 0.81f)
        return 2;
    if (cos_bitnet_native_top2_gap_fraction(0, 0, &s) != -1)
        return 3;
    if (cos_bitnet_native_try_complete("x", NULL, 0, NULL) != -1)
        return 4;
    return 0;
}
