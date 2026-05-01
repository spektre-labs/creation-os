/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "introspection.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_ultra_metacog_self_test();
    cos_ultra_metacog_levels_t demo = {
        .sigma_perception = 0.08f,
        .sigma_self = 0.15f,
        .sigma_social = 0.42f,
        .sigma_situational = 0.12f,
    };
    cos_ultra_metacog_emit_report(stdout, &demo);
    return 0;
}
