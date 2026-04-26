/*
 * cos-validate — validate a σ-credential JSON sidecar.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "c2pa_sigma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, "usage: cos-validate SIDEcar.json\n");
        return 2;
    }
    return cos_c2pa_sigma_validate_file(argv[1], stdout) != 0 ? 1 : 0;
}
