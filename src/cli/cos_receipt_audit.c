/*
 * cos-receipt-audit — verify append-only ~/.cos/audit/YYYY-MM-DD.jsonl chains.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "proof_receipt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char        pathbuf[768];
    const char *path;
    int         rc;

    if (argc >= 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        fprintf(stdout,
                "usage: cos-receipt-audit [AUDIT.jsonl]\n"
                "  default   verify today's UTC ~/.cos/audit/YYYY-MM-DD.jsonl\n"
                "  COS_AUDIT_DIR overrides the directory (file name still "
                "YYYY-MM-DD.jsonl)\n");
        return 0;
    }

    if (argc >= 2)
        path = argv[1];
    else if (cos_proof_receipt_audit_default_path(pathbuf, sizeof pathbuf)
             == 0)
        path = pathbuf;
    else {
        fprintf(stderr, "cos-receipt-audit: cannot resolve audit path "
                        "(HOME unset?)\n");
        return 2;
    }

    fprintf(stdout, "cos-receipt-audit: %s\n", path);
    rc = cos_proof_receipt_audit_verify_jsonl(path, stdout);
    if (rc != 0)
        fprintf(stderr, "cos-receipt-audit: VERIFY FAILED (rc=%d)\n", rc);
    else
        fprintf(stdout, "cos-receipt-audit: OK\n");
    return rc != 0 ? 1 : 0;
}
