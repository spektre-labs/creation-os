/* SPDX-License-Identifier: AGPL-3.0-or-later */
#if defined(__APPLE__)
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/types.h>

bool cos_runtime_has_sme(void)
{
    static const char *const keys[] = {
        "hw.optional.arm.FEAT_SME",
        /* Some Apple Silicon OS builds expose SME2 separately when present */
        "hw.optional.arm.FEAT_SME2",
    };
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        int v = 0;
        size_t sz = sizeof(v);
        if (sysctlbyname(keys[k], &v, &sz, NULL, 0) == 0 && v != 0)
            return true;
    }
    return false;
}
#else
#include <stdbool.h>
bool cos_runtime_has_sme(void)
{
    return false;
}
#endif
