/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* Pattern-layer “SMT” facade — wraps constitution checks. */

#include "codex_smt.h"

#include "../sigma/constitution.h"

#include <string.h>

int cos_codex_smt_evaluate(const char *response, float sigma, char *detail,
                           size_t detail_cap, int *violations_out,
                           int *mandatory_violations_out)
{
    if (violations_out)
        *violations_out = 0;
    if (mandatory_violations_out)
        *mandatory_violations_out = 0;
    if (detail != NULL && detail_cap > 0)
        detail[0] = '\0';
    return cos_constitution_check(response, sigma, NULL, violations_out,
                                  mandatory_violations_out, detail,
                                  detail != NULL ? (int)detail_cap : 0);
}

int cos_codex_smt_self_test(void)
{
    int v, mv;
    char buf[128];

    buf[0] = '\0';
    if (cos_codex_smt_evaluate("plain answer", 0.3f, buf, sizeof buf, &v, &mv)
        != 0)
        return 1;
    return 0;
}
