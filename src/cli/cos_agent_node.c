/*
 * σ-native agent node surface (MCP + A2A identity + quarantine view).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "cos_agent_node.h"

#include "a2a.h"
#include "codex.h"
#include "license_attest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void hex64(const uint8_t b[32], char out[65])
{
    spektre_hex_lower(b, out);
}

int cos_agent_identity_fill(struct cos_agent_identity *out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->agent_name, sizeof out->agent_name, "%s", "creation-os");

    cos_sigma_codex_t cx;
    memset(&cx, 0, sizeof cx);
    if (cos_sigma_codex_load(NULL, &cx) == 0 && cx.bytes && cx.size > 0) {
        spektre_sha256(cx.bytes, cx.size, out->codex_hash);
        cos_sigma_codex_free(&cx);
    } else {
        spektre_sha256("", 0, out->codex_hash);
        if (cx.bytes)
            cos_sigma_codex_free(&cx);
    }

    memcpy(out->license_hash, spektre_license_sha256_bin,
           sizeof out->license_hash);

    FILE *lf = fopen("LICENSE-SCSL-1.0.md", "rb");
    if (!lf)
        lf = fopen("../LICENSE-SCSL-1.0.md", "rb");
    if (lf) {
        if (fseek(lf, 0, SEEK_END) == 0) {
            long sz = ftell(lf);
            if (sz > 0 && sz < (1 << 22)) {
                rewind(lf);
                char *buf = (char *)malloc((size_t)sz + 1u);
                if (buf && fread(buf, 1, (size_t)sz, lf) == (size_t)sz) {
                    spektre_sha256(buf, (size_t)sz, out->license_hash);
                }
                free(buf);
            }
        }
        fclose(lf);
    }

    out->quarantined_count = cos_a2a_quarantine_count();
    out->sigma_mean_lifetime = 0.f;
    out->total_interactions = 0;
    return 0;
}

static int cmd_start(void)
{
    fprintf(stderr,
            "σ-native agent: MCP JSON-RPC on stdio (creation_os_mcp).\n"
            "Pair with `cos a2a` / peer cards for Agent2Agent trust.\n");
    fflush(stderr);
    execlp("./creation_os_mcp", "creation_os_mcp", (char *)NULL);
    execlp("creation_os_mcp", "creation_os_mcp", (char *)NULL);
    fprintf(stderr, "cos agent start: exec creation_os_mcp failed: %s\n",
            strerror(errno));
    return 2;
}

static int cmd_status(void)
{
    struct cos_agent_identity id;
    cos_agent_identity_fill(&id);
    char hc[65], hl[65];
    hex64(id.codex_hash, hc);
    hex64(id.license_hash, hl);
    printf("{\n");
    printf("  \"agent_name\": \"%s\",\n", id.agent_name);
    printf("  \"codex_sha256\": \"%s\",\n", hc);
    printf("  \"license_sha256\": \"%s\",\n", hl);
    printf("  \"sigma_mean_lifetime\": %.4f,\n",
           (double)id.sigma_mean_lifetime);
    printf("  \"total_interactions\": %d,\n", id.total_interactions);
    printf("  \"quarantined_count\": %d\n", id.quarantined_count);
    printf("}\n");
    return 0;
}

static int cmd_connections(void)
{
    char path[1024];
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        fprintf(stderr, "cos agent connections: HOME unset\n");
        return 2;
    }
    snprintf(path, sizeof path, "%s/.cos/a2a.json", home);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("# %s (missing)\n", path);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0 || sz > 262144) {
        fclose(fp);
        fprintf(stderr, "cos agent connections: bad state size\n");
        return 2;
    }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(fp);
        return 3;
    }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    buf[rd] = '\0';
    fclose(fp);
    printf("# %s\n", path);
    fputs(buf, stdout);
    if (rd == 0 || buf[rd - 1] != '\n')
        fputc('\n', stdout);
    free(buf);
    return 0;
}

static int cmd_quarantine(void)
{
    cos_a2a_quarantine_print(stdout);
    return 0;
}

int cos_agent_node_cli(int argc, char **argv)
{
    if (argc < 2)
        return COS_AGENT_NODE_NA;
    const char *sub = argv[1];
    if (strcmp(sub, "start") == 0)
        return cmd_start();
    if (strcmp(sub, "status") == 0)
        return cmd_status();
    if (strcmp(sub, "connections") == 0)
        return cmd_connections();
    if (strcmp(sub, "quarantine") == 0)
        return cmd_quarantine();
    if (strcmp(sub, "--self-test-node") == 0)
        return cos_agent_node_self_test();
    return COS_AGENT_NODE_NA;
}

int cos_agent_node_self_test(void)
{
    struct cos_agent_identity id;
    if (cos_agent_identity_fill(&id) != 0)
        return 1;
    char hc[65];
    hex64(id.codex_hash, hc);
    if (strlen(hc) != 64u)
        return 2;
    return 0;
}
