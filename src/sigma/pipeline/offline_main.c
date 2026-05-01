/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-offline demo — prepare a ~/.cos.demo sandbox, verify, report.
 *
 * The demo writes five small files into a per-pid temp directory
 * (so two concurrent invocations don't race), runs the verifier
 * twice (full + one file knocked out), and prints both JSON
 * reports.  A real `cos offline prepare` would call the same
 * API on ~/.cos/.
 */
#define _GNU_SOURCE
#include "offline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_small(const char *path, const char *body) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = strlen(body);
    if (fwrite(body, 1, n, f) != n) { fclose(f); return -2; }
    fclose(f);
    return 0;
}

int main(void) {
    char dir[256];
    snprintf(dir, sizeof dir, "/tmp/cos_offline_demo_%d", (int)getpid());
    mkdir(dir, 0700);

    char p[512];
    snprintf(p, sizeof p, "%s/model.bin",    dir); write_small(p, "BITNET-2B-4T-SHARD");
    snprintf(p, sizeof p, "%s/engram.sqlite",dir); write_small(p, "SQLITE-3.45-ENGRAM");
    snprintf(p, sizeof p, "%s/rag_index.bin",dir); write_small(p, "COS-RAG-v1-INDEX");
    snprintf(p, sizeof p, "%s/codex.txt",    dir); write_small(p, "assert(declared == realized); 1 == 1;");
    snprintf(p, sizeof p, "%s/persona.json", dir); write_small(p, "{\"name\":\"athena_user\",\"language\":\"fi\"}");

    cos_offline_plan_t plan;
    cos_offline_plan_default(&plan, dir);

    cos_offline_report_t full;
    cos_offline_verify(&plan, &full);

    /* Now remove the rag index and re-verify to show degradation. */
    snprintf(p, sizeof p, "%s/rag_index.bin", dir);
    remove(p);
    cos_offline_report_t degraded;
    cos_offline_verify(&plan, &degraded);

    char buf_full[4096];
    char buf_deg[4096];
    cos_offline_report_json(&full,     buf_full, sizeof buf_full);
    cos_offline_report_json(&degraded, buf_deg,  sizeof buf_deg);

    int self_rc = cos_offline_self_test();

    printf("{\n");
    printf("  \"kernel\": \"sigma_offline\",\n");
    printf("  \"self_test\": %d,\n", self_rc);
    printf("  \"dir\": \"%s\",\n", dir);
    printf("  \"full\": %s,\n", buf_full);
    printf("  \"after_rm_rag\": %s\n", buf_deg);
    printf("}\n");

    /* Tear the demo sandbox down. */
    snprintf(p, sizeof p, "%s/model.bin",    dir); remove(p);
    snprintf(p, sizeof p, "%s/engram.sqlite",dir); remove(p);
    snprintf(p, sizeof p, "%s/codex.txt",    dir); remove(p);
    snprintf(p, sizeof p, "%s/persona.json", dir); remove(p);
    rmdir(dir);
    return 0;
}
