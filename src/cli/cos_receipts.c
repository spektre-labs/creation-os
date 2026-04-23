/*
 * cos receipts — list / verify / export proof receipt chain.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "proof_receipt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int receipts_dir(char *buf, size_t cap)
{
    const char *env = getenv("COS_RECEIPTS_DIR");
    const char *home = getenv("HOME");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    if (home == NULL || home[0] == '\0')
        return -1;
    snprintf(buf, cap, "%s/.cos/receipts", home);
    return 0;
}

int main(int argc, char **argv)
{
    char dir[512];
    int  verify = 0;
    int  export_jsonl = 0;
    int  i;

    if (receipts_dir(dir, sizeof dir) != 0) {
        fprintf(stderr, "cos receipts: HOME unset\n");
        return 2;
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verify") == 0)
            verify = 1;
        else if (strcmp(argv[i], "--export") == 0)
            export_jsonl = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stdout,
                    "usage: cos receipts [--verify] [--export]\n"
                    "  default   list receipt JSON files\n"
                    "  --verify  SHA-256 verify each persisted receipt\n"
                    "  --export  concatenate JSON records to stdout\n"
                    "  COS_RECEIPTS_DIR overrides ~/.cos/receipts\n");
            return 0;
        }
    }

    if (verify) {
        int rc = cos_proof_receipt_chain_verify_dir(dir);
        if (rc == 1) {
            fprintf(stdout, "(no receipts in %s)\n", dir);
            return 0;
        }
        if (rc != 0) {
            fprintf(stderr, "cos receipts: verify FAILED (code %d)\n", rc);
            return 1;
        }
        fprintf(stdout, "cos receipts: verify OK (%s)\n", dir);
        return 0;
    }

    if (export_jsonl) {
        char *blob = cos_proof_receipt_chain_export_jsonl(dir);
        if (blob == NULL) {
            fprintf(stderr, "cos receipts: export failed\n");
            return 3;
        }
        fputs(blob, stdout);
        free(blob);
        return 0;
    }

    fprintf(stdout, "Proof receipts in %s\n", dir);
    cos_proof_receipt_print_filenames(stdout, dir);
    return 0;
}
