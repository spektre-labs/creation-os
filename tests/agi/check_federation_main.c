/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "../../src/sigma/federation.h"

#include <stdlib.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_federation_config d;
    if (cos_federation_default_config(&d) != 0)
        return 1;
    if (d.min_trust_to_accept <= 0.f || d.min_sigma_to_share <= 0.f)
        return 2;
    return cos_federation_self_test() != 0 ? 3 : 0;
}
