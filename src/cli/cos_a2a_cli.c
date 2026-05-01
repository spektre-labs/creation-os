/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* cos-a2a — product-level CLI over the σ-A2A kernel (OMEGA-2).
 *
 * Google A2A (Agent2Agent) is a 2026 standard for capability-gated
 * agent-to-agent task dispatch with per-peer trust tracking.  The
 * σ-A2A kernel (src/sigma/pipeline/a2a.c) ships the state machine;
 * this CLI turns it into something you can drive from the shell and
 * persist between sessions in ~/.cos/a2a.json.
 *
 * Commands:
 *
 *   cos-a2a card                              print this agent's card JSON
 *   cos-a2a register <id> <url> <caps...>     add or overwrite a peer
 *   cos-a2a request <id> <cap> <sigma>        record a task outcome
 *                                             (sigma in [0,1]; updates σ_trust EMA)
 *   cos-a2a list                              list peers with σ_trust + blocklist
 *   cos-a2a demo                              run the canonical 5-exchange script
 *   cos-a2a reset                             drop the persisted state
 *
 * State: ~/.cos/a2a.json (override with COS_A2A_STATE=/abs/path).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "a2a.h"
#include "cos_version.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* =====================================================================
 * Persistence path
 * ===================================================================== */

static int a2a_default_path(char *buf, size_t cap) {
    const char *env = getenv("COS_A2A_STATE");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') return -1;
    snprintf(buf, cap, "%s/.cos", home);
    /* Create dir if missing; harmless if already present. */
    mkdir(buf, 0700);
    snprintf(buf, cap, "%s/.cos/a2a.json", home);
    return 0;
}

/* =====================================================================
 * Tiny JSON persistence.  Deliberately hand-rolled, bounded to the
 * exact shape `cos-a2a list` prints, so the kernel has zero extra
 * deps.
 * ===================================================================== */

static void emit_json_string(FILE *fp, const char *s) {
    fputc('"', fp);
    if (s != NULL) {
        for (; *s; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') { fputc('\\', fp); fputc((int)c, fp); }
            else if (c < 0x20u)        { fprintf(fp, "\\u%04x", (unsigned)c); }
            else                        { fputc((int)c, fp); }
        }
    }
    fputc('"', fp);
}

static int a2a_state_save(const cos_a2a_network_t *net, const char *path) {
    FILE *fp = fopen(path, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "{\n  \"schema\":\"cos_a2a_v1\",\n  \"self_id\":");
    emit_json_string(fp, net->self_id);
    fprintf(fp, ",\n  \"tau_block\":%.4f,\n  \"lr\":%.4f,\n  \"peers\":[\n",
            (double)net->tau_block, (double)net->lr);
    for (int i = 0; i < net->n_peers; ++i) {
        const cos_a2a_peer_t *p = &net->peers[i];
        fprintf(fp, "    {\"id\":");
        emit_json_string(fp, p->agent_id);
        fprintf(fp, ",\"url\":");
        emit_json_string(fp, p->agent_card_url);
        fprintf(fp,
                ",\"sigma_trust\":%.4f,\"exchanges_total\":%d,"
                "\"exchanges_blocked\":%d,\"blocklisted\":%d,"
                "\"sigma_gate_declared\":%d,\"scsl_compliant\":%d,"
                "\"capabilities\":[",
                (double)p->sigma_trust, p->exchanges_total,
                p->exchanges_blocked, p->blocklisted,
                p->sigma_gate_declared, p->scsl_compliant);
        for (int j = 0; j < p->n_capabilities; ++j) {
            if (j > 0) fputc(',', fp);
            emit_json_string(fp, p->capabilities[j]);
        }
        fprintf(fp, "]}%s\n", (i + 1 < net->n_peers) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return 0;
}

/* Hand-rolled tokeniser: walks "key":value pairs at a bounded depth.
 * Matches the schema above.  Tolerant of whitespace and trailing
 * commas; not a full JSON parser. */
static const char *find_key(const char *s, const char *k) {
    if (s == NULL || k == NULL) return NULL;
    size_t kl = strlen(k);
    const char *p = s;
    while ((p = strchr(p, '"')) != NULL) {
        const char *q = p + 1;
        if (strncmp(q, k, kl) == 0 && q[kl] == '"') {
            q += kl + 1;
            while (*q && isspace((unsigned char)*q)) q++;
            if (*q == ':') { q++; while (*q && isspace((unsigned char)*q)) q++; return q; }
        }
        p++;
    }
    return NULL;
}

static int copy_js_string(const char *v, char *dst, size_t cap) {
    if (v == NULL || *v != '"' || dst == NULL || cap == 0) return -1;
    v++;
    size_t w = 0;
    while (*v && *v != '"' && w + 1 < cap) {
        if (*v == '\\' && v[1] != '\0') {
            dst[w++] = v[1];
            v += 2;
        } else {
            dst[w++] = *v++;
        }
    }
    dst[w] = '\0';
    return (*v == '"') ? 0 : -1;
}

static int a2a_state_load(cos_a2a_network_t *net, const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > 256 * 1024) { fclose(fp); return -1; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (buf == NULL) { fclose(fp); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    buf[rd] = '\0';
    fclose(fp);

    /* Extract self_id so we preserve it across sessions. */
    char self_id[COS_A2A_ID_MAX];
    const char *v_self = find_key(buf, "self_id");
    if (v_self == NULL || copy_js_string(v_self, self_id, sizeof(self_id)) != 0) {
        snprintf(self_id, sizeof(self_id), "creation-os");
    }
    cos_a2a_init(net, self_id);

    /* Scan `peers`: look for each `{"id":` repeatedly. */
    const char *p = strstr(buf, "\"peers\":");
    if (p == NULL) { free(buf); return 0; }
    const char *q = p;
    while ((q = strstr(q, "{\"id\":")) != NULL) {
        char id[COS_A2A_ID_MAX]     = {0};
        char url[COS_A2A_URL_MAX]   = {0};
        char caps[COS_A2A_CAPS_PER_PEER][COS_A2A_CAP_MAX];
        for (int i = 0; i < COS_A2A_CAPS_PER_PEER; ++i) caps[i][0] = '\0';
        int  n_caps = 0;
        float trust = COS_A2A_TRUST_INIT;
        int ex_total = 0, ex_block = 0, block = 0, sg = 0, scsl = 0;

        /* Block boundaries: find the matching closing brace. */
        const char *obj_end = strchr(q, '}');
        if (obj_end == NULL) break;
        /* Restrict our find_key scans to this object by NUL-terminating
         * a temporary copy.  Cheap because objects are < 1 KB. */
        size_t obj_len = (size_t)(obj_end - q + 1);
        if (obj_len > 4000) obj_len = 4000;
        char obj[4001];
        memcpy(obj, q, obj_len);
        obj[obj_len] = '\0';

        const char *v;
        v = find_key(obj, "id");           if (v) copy_js_string(v, id,  sizeof(id));
        v = find_key(obj, "url");          if (v) copy_js_string(v, url, sizeof(url));
        v = find_key(obj, "sigma_trust");  if (v) trust = (float)strtod(v, NULL);
        v = find_key(obj, "exchanges_total");   if (v) ex_total = atoi(v);
        v = find_key(obj, "exchanges_blocked"); if (v) ex_block = atoi(v);
        v = find_key(obj, "blocklisted");       if (v) block = atoi(v);
        v = find_key(obj, "sigma_gate_declared"); if (v) sg   = atoi(v);
        v = find_key(obj, "scsl_compliant");      if (v) scsl = atoi(v);

        /* Capabilities: scan ["..","..",...]. */
        const char *vc = find_key(obj, "capabilities");
        if (vc != NULL && *vc == '[') {
            vc++;
            while (*vc && *vc != ']' && n_caps < COS_A2A_CAPS_PER_PEER) {
                while (*vc && isspace((unsigned char)*vc)) vc++;
                if (*vc == '"') {
                    if (copy_js_string(vc, caps[n_caps], COS_A2A_CAP_MAX) == 0)
                        n_caps++;
                    vc++;
                    while (*vc && *vc != '"') {
                        if (*vc == '\\' && vc[1] != '\0') vc += 2;
                        else vc++;
                    }
                    if (*vc == '"') vc++;
                }
                while (*vc && *vc != ',' && *vc != ']') vc++;
                if (*vc == ',') vc++;
            }
        }

        const char *cap_ptrs[COS_A2A_CAPS_PER_PEER];
        for (int i = 0; i < n_caps; ++i) cap_ptrs[i] = caps[i];
        if (id[0] != '\0') {
            int rc = cos_a2a_peer_register(net, id, url,
                                           cap_ptrs, n_caps, sg, scsl);
            if (rc == COS_A2A_OK) {
                cos_a2a_peer_t *pp = cos_a2a_peer_find(net, id);
                if (pp != NULL) {
                    pp->sigma_trust       = trust;
                    pp->exchanges_total   = ex_total;
                    pp->exchanges_blocked = ex_block;
                    pp->blocklisted       = block;
                }
            }
        }
        q = obj_end + 1;
    }
    free(buf);
    return 0;
}

/* =====================================================================
 * Commands
 * ===================================================================== */

static void usage(void) {
    fprintf(stderr,
        "cos-a2a — agent-to-agent with σ-trust (Creation OS %s)\n"
        "  cos-a2a card\n"
        "  cos-a2a register <id> <url> [capability ...]\n"
        "  cos-a2a request  <id> <capability> <sigma>\n"
        "  cos-a2a list\n"
        "  cos-a2a quarantine        print σ-quarantined inbound messages (process-local)\n"
        "  cos-a2a demo\n"
        "  cos-a2a reset\n"
        "  cos-a2a --self <id>         override self_id (default: creation-os)\n"
        "  cos-a2a --help\n"
        "\n"
        "state file: $COS_A2A_STATE or ~/.cos/a2a.json\n",
        COS_VERSION_STRING);
}

static int cmd_card(const cos_a2a_network_t *net) {
    char buf[2048];
    /* Default capability set for Creation OS. */
    const char *caps[] = { "qa", "code", "sigma_gate", "conformal" };
    int rc = cos_a2a_self_card(net, caps, (int)(sizeof caps / sizeof caps[0]),
                               buf, sizeof(buf));
    /* cos_a2a_self_card returns bytes written (> 0) on success and a
     * negative COS_A2A_ERR_* on failure; treat any non-negative value
     * as success. */
    if (rc < 0) return 1;
    fputs(buf, stdout);
    if (buf[0] != '\0' && buf[strlen(buf) - 1] != '\n') fputc('\n', stdout);
    return 0;
}

static int cmd_register(cos_a2a_network_t *net, int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "register: need <id> <url> [capabilities...]\n");
        return 2;
    }
    const char *id  = argv[0];
    const char *url = argv[1];
    const char *caps[COS_A2A_CAPS_PER_PEER];
    int n = argc - 2;
    if (n > COS_A2A_CAPS_PER_PEER) n = COS_A2A_CAPS_PER_PEER;
    for (int i = 0; i < n; ++i) caps[i] = argv[2 + i];
    int rc = cos_a2a_peer_register(net, id, url, caps, n,
                                   /* sigma_gate_declared */ 1,
                                   /* scsl_compliant      */ 1);
    if (rc != COS_A2A_OK) {
        fprintf(stderr, "register: failed (rc=%d, network full or bad args)\n", rc);
        return 3;
    }
    fprintf(stdout, "registered %s @ %s (%d capabilit%s)\n",
            id, url, n, n == 1 ? "y" : "ies");
    return 0;
}

static int cmd_request(cos_a2a_network_t *net, int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "request: need <id> <capability> <sigma>\n");
        return 2;
    }
    const char *id  = argv[0];
    const char *cap = argv[1];
    float sigma = (float)strtod(argv[2], NULL);
    if (sigma < 0.0f) sigma = 0.0f;
    if (sigma > 1.0f) sigma = 1.0f;

    int rc = cos_a2a_request(net, id, cap, sigma);
    cos_a2a_peer_t *p = cos_a2a_peer_find(net, id);
    const char *status =
        (rc == COS_A2A_OK)           ? "OK" :
        (rc == COS_A2A_ERR_BLOCKED)  ? "BLOCKED" :
        (rc == COS_A2A_ERR_NOPEER)   ? "NO_PEER" :
        (rc == COS_A2A_ERR_CAP)      ? "NO_CAP" :
                                       "ERR";
    if (p != NULL) {
        fprintf(stdout,
            "%-8s  peer=%-24s cap=%-16s σ_resp=%.3f → σ_trust=%.3f "
            "(exchanges=%d, blocklisted=%s)\n",
            status, id, cap, (double)sigma,
            (double)p->sigma_trust,
            p->exchanges_total,
            p->blocklisted ? "true" : "false");
    } else {
        fprintf(stdout, "%-8s  peer=%s cap=%s σ=%.3f\n",
                status, id, cap, (double)sigma);
    }
    return (rc == COS_A2A_OK || rc == COS_A2A_ERR_BLOCKED) ? 0 : 4;
}

static int cmd_quarantine(void)
{
    cos_a2a_quarantine_print(stdout);
    return 0;
}

static int cmd_list(const cos_a2a_network_t *net) {
    fprintf(stdout,
        "peers: %d · τ_block=%.2f · lr=%.2f\n"
        "  %-24s  %-32s  %-10s %-8s %-6s\n",
        net->n_peers, (double)net->tau_block, (double)net->lr,
        "ID", "URL", "σ_trust", "exchanges", "block?");
    for (int i = 0; i < net->n_peers; ++i) {
        const cos_a2a_peer_t *p = &net->peers[i];
        fprintf(stdout,
            "  %-24s  %-32s  %-10.3f %-8d %-6s\n",
            p->agent_id, p->agent_card_url,
            (double)p->sigma_trust, p->exchanges_total,
            p->blocklisted ? "YES" : "-");
    }
    return 0;
}

/* Canonical 5-exchange demo: peer-A stays honest, peer-B drifts
 * toward high σ and trips the blocklist threshold; CI diffs the
 * deterministic output. */
static int cmd_demo(cos_a2a_network_t *net) {
    cos_a2a_init(net, "creation-os");
    const char *caps_a[] = { "qa", "code" };
    const char *caps_b[] = { "forecast", "news" };
    cos_a2a_peer_register(net, "alice",
                          "https://alice.example/.well-known/agent.json",
                          caps_a, 2, 1, 1);
    cos_a2a_peer_register(net, "bob",
                          "https://bob.example/.well-known/agent.json",
                          caps_b, 2, 0, 0);

    /* 12 exchanges: alice stays honest (σ ≈ 0.10), bob is a hallucinator
     * (σ ≈ 0.95).  With lr=0.20 and τ_block=0.90 the EMA crosses τ at
     * roughly the tenth bob exchange, reliably tripping the blocklist. */
    struct { const char *id; const char *cap; float sigma; } ex[] = {
        { "alice", "qa",       0.10f },
        { "alice", "qa",       0.12f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
        { "bob",   "forecast", 0.95f },
    };
    fprintf(stdout, "σ-A2A demo (canonical 12-exchange script)\n");
    for (size_t i = 0; i < sizeof(ex)/sizeof(ex[0]); ++i) {
        int rc = cos_a2a_request(net, ex[i].id, ex[i].cap, ex[i].sigma);
        cos_a2a_peer_t *p = cos_a2a_peer_find(net, ex[i].id);
        const char *status =
            (rc == COS_A2A_OK)          ? "OK"      :
            (rc == COS_A2A_ERR_BLOCKED) ? "BLOCKED" : "ERR";
        fprintf(stdout,
            "  ex%zu %-7s peer=%-6s cap=%-10s σ=%.2f → trust=%.3f %s\n",
            i + 1, status, ex[i].id, ex[i].cap,
            (double)ex[i].sigma,
            p != NULL ? (double)p->sigma_trust : 0.0,
            p != NULL && p->blocklisted ? "[blocklisted]" : "");
    }
    fprintf(stdout, "\n");
    return cmd_list(net);
}

int main(int argc, char **argv) {
    const char *self_id = NULL;
    int consumed = 1;
    while (consumed < argc) {
        if (strcmp(argv[consumed], "--help") == 0) { usage(); return 0; }
        if (strcmp(argv[consumed], "--quarantine") == 0) {
            cos_a2a_quarantine_print(stdout);
            return 0;
        }
        if (strcmp(argv[consumed], "--self") == 0 && consumed + 1 < argc) {
            self_id = argv[consumed + 1];
            consumed += 2;
            continue;
        }
        break;
    }
    if (consumed >= argc) {
        usage();
        return 1;
    }
    const char *cmd = argv[consumed++];

    char path[1024];
    if (a2a_default_path(path, sizeof(path)) != 0) {
        fprintf(stderr, "cos-a2a: cannot resolve state path (HOME unset?)\n");
        return 2;
    }

    cos_a2a_network_t net;
    cos_a2a_init(&net, self_id != NULL ? self_id : "creation-os");

    /* Load existing state (ignore failure — fresh network is fine). */
    if (strcmp(cmd, "reset") != 0 && strcmp(cmd, "demo") != 0) {
        a2a_state_load(&net, path);
        if (self_id != NULL) {
            /* Override self_id from CLI even after load. */
            snprintf(net.self_id, sizeof(net.self_id), "%s", self_id);
        }
    }

    int rc = 0;
    int save = 0;
    if      (strcmp(cmd, "card")     == 0) rc = cmd_card(&net);
    else if (strcmp(cmd, "list")     == 0) rc = cmd_list(&net);
    else if (strcmp(cmd, "quarantine") == 0) rc = cmd_quarantine();
    else if (strcmp(cmd, "register") == 0) { rc = cmd_register(&net, argc - consumed, argv + consumed); save = (rc == 0); }
    else if (strcmp(cmd, "request")  == 0) { rc = cmd_request (&net, argc - consumed, argv + consumed); save = (rc == 0); }
    else if (strcmp(cmd, "demo")     == 0) rc = cmd_demo(&net);
    else if (strcmp(cmd, "reset")    == 0) {
        unlink(path);
        fprintf(stdout, "reset: %s removed\n", path);
    } else {
        fprintf(stderr, "cos-a2a: unknown command '%s'\n", cmd);
        usage();
        rc = 3;
    }

    if (save) {
        if (a2a_state_save(&net, path) != 0) {
            fprintf(stderr, "cos-a2a: warning — could not persist to %s\n", path);
        }
    }
    return rc;
}
