/* SPDX-License-Identifier: AGPL-3.0-or-later */
#if defined(__APPLE__)
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/types.h>

bool cos_runtime_has_sme(void)
{
    int v = 0;
    size_t sz = sizeof(v);
    if (sysctlbyname("hw.optional.arm.FEAT_SME", &v, &sz, NULL, 0) == 0)
        return v != 0;
    return false;
}
#else
#include <stdbool.h>
bool cos_runtime_has_sme(void)
{
    return false;
}
#endif
