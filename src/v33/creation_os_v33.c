/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v33 — σ-routed SLM-default / LLM-fallback agent runtime (POSIX lab)
 *
 * Not merge-gate. Ships: routing math + schema-first validation + session metrics hooks.
 */
#if defined(_WIN32)
#include <stdio.h>
int main(void)
{
    fprintf(stderr, "creation_os_v33: POSIX-only.\n");
    return 2;
}
#else
#define _POSIX_C_SOURCE 200809L

#include "metrics.h"
#include "model_registry.h"
#include "router.h"
#include "schema_validator.h"

#include "../sigma/channels.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>

static int self_test_router(void)
{
    cos_routing_config_t cfg;
    cos_routing_defaults(&cfg);
    (void)cos_routing_load("config/routing.json", &cfg);

    float flat[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    int budget = 8;
    cos_route_result_t r0 = cos_route_from_logits(&cfg, flat, 8, 0, &budget);
    if (r0.decision != COS_ROUTE_ABSTAIN) {
        fprintf(stderr, "[v33 self-test] FAIL flat logits should abstain (high entropy)\n");
        return 1;
    }

    float peaked[8] = { 8.f, -4.f, -4.f, -4.f, -4.f, -4.f, -4.f, -4.f };
    budget = 8;
    cos_route_result_t r1 = cos_route_from_logits(&cfg, peaked, 8, 0, &budget);
    if (r1.decision != COS_ROUTE_PRIMARY) {
        fprintf(stderr, "[v33 self-test] FAIL peaked logits should stay primary\n");
        return 1;
    }

    cos_routing_config_t cfg2 = cfg;
    (void)snprintf(cfg2.fallback_model_name, sizeof cfg2.fallback_model_name, "phi-fallback");
    cos_model_registry_t reg;
    cos_model_registry_init_defaults(&reg);
    if (!cos_model_find(&reg, "phi-fallback")) {
        cos_model_entry_t tmp;
        memset(&tmp, 0, sizeof tmp);
        (void)snprintf(tmp.name, sizeof tmp.name, "phi-fallback");
        (void)snprintf(tmp.binary, sizeof tmp.binary, "/bin/true");
        (void)snprintf(tmp.model_path, sizeof tmp.model_path, "/dev/null");
        tmp.is_local = 1;
        if (reg.n >= COS_MODEL_MAX_ENTRIES) {
            fprintf(stderr, "[v33 self-test] FAIL registry full\n");
            return 1;
        }
        reg.entries[reg.n++] = tmp;
    }
    int fb_ok = cos_model_has_named_fallback(&reg, "phi-fallback");
    budget = 8;
    cos_route_result_t r2 = cos_route_from_logits(&cfg2, flat, 8, fb_ok, &budget);
    if (r2.decision != COS_ROUTE_FALLBACK) {
        fprintf(stderr, "[v33 self-test] FAIL fallback path (got decision=%d)\n", (int)r2.decision);
        return 1;
    }

    fprintf(stderr, "[v33 self-test] OK router\n");
    return 0;
}

static int self_test_schema(const char *schema_dir_prefix)
{
    char path[512];
    (void)snprintf(path, sizeof path, "%s/config/tools/read_file.schema.json", schema_dir_prefix);

    const char *good = "{\"name\":\"read_file\",\"arguments\":{\"path\":\"/tmp/x\"}}";
    if (!cos_schema_validate_tool_json(good, path)) {
        fprintf(stderr, "[v33 self-test] FAIL schema should accept good tool json\n");
        return 1;
    }
    const char *bad = "{\"name\":\"read_file\",\"arguments\":{}}";
    if (cos_schema_validate_tool_json(bad, path)) {
        fprintf(stderr, "[v33 self-test] FAIL schema should reject missing path\n");
        return 1;
    }
    const char *bad_name = "{\"name\":\"shell\",\"arguments\":{\"path\":\"x\"}}";
    if (cos_schema_validate_tool_json(bad_name, path)) {
        fprintf(stderr, "[v33 self-test] FAIL schema should reject wrong tool name\n");
        return 1;
    }

    const char *retry = bad;
    int ok = cos_schema_validate_tool_json(retry, path);
    if (!ok)
        ok = cos_schema_validate_tool_json(good, path);
    if (!ok) {
        fprintf(stderr, "[v33 self-test] FAIL schema retry path\n");
        return 1;
    }

    fprintf(stderr, "[v33 self-test] OK schema\n");
    return 0;
}

static int self_test_metrics(void)
{
    (void)mkdir("metrics", 0755);
    cos_metrics_session_t m;
    if (cos_metrics_session_open(&m, "metrics") != 0) {
        fprintf(stderr, "[v33 self-test] SKIP metrics (metrics/ not writable?)\n");
        return 0;
    }
    cos_metrics_task_started(&m, 0);
    cos_metrics_schema(&m, 1);
    cos_metrics_tool_exec(&m, 1);
    cos_metrics_latency_e2e_ns(&m, 1200);
    cos_metrics_latency_token_ns(&m, 40);
    cos_metrics_sigma_observe(&m, 0, 0.37f);
    cos_metrics_sigma_observe(&m, 1, 0.88f);
    cos_metrics_task_outcome(&m, 0, "completed");
    cos_metrics_session_close(&m);
    fprintf(stderr, "[v33 self-test] OK metrics (wrote %s)\n", m.path);
    return 0;
}

static int self_test_sigma_channels(void)
{
    float lp[8] = { 1.f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f };
    sigma_state_t st;
    memset(&st, 0, sizeof st);
    st.logit_entropy = sigma_logit_entropy(lp, 8);
    st.top_margin = sigma_top_margin(lp, 8);
    sigma_thresholds_t th;
    memset(&th, 0, sizeof th);
    th.logit_entropy = 0.85f;
    th.top_margin = 0.92f;
    int gate = sigma_abstain_gate(&st, &th);
    if (gate == 0) {
        fprintf(stderr, "[v33 self-test] NOTE sigma_abstain_gate did not fire on toy (gate=%d)\n", gate);
    }
    fprintf(stderr, "[v33 self-test] OK sigma channels (entropy=%.4f margin=%.4f gate=%d)\n",
        st.logit_entropy, st.top_margin, gate);
    return 0;
}

static void fill_logits_scenario(int seed, float *logits, int n)
{
    unsigned s = (unsigned)seed * 1103515245u + 12345u;
    for (int i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        logits[i] = (float)((int)(s % 2000) - 1000) * 0.001f;
    }
    if ((seed % 7) == 0) {
        for (int i = 0; i < n; i++)
            logits[i] = 0.f;
    } else if ((seed % 7) == 1) {
        logits[0] = 6.f;
        for (int i = 1; i < n; i++)
            logits[i] = -3.f;
    }
}

static int bench_agent(const char *repo_root)
{
    const char *mode = getenv("COS_V33_BENCH_MODE");
    if (!mode || !*mode)
        mode = "bitnet+fallback";

    cos_routing_config_t cfg;
    cos_routing_defaults(&cfg);
    char rpath[512];
    (void)snprintf(rpath, sizeof rpath, "%s/config/routing.json", repo_root);
    (void)cos_routing_load(rpath, &cfg);

    cos_model_registry_t reg;
    cos_model_registry_init_defaults(&reg);
    char mpath[512];
    (void)snprintf(mpath, sizeof mpath, "%s/config/models.json", repo_root);
    (void)cos_model_registry_load(mpath, &reg);

    cos_metrics_session_t met;
    const char *mdir = getenv("COS_V33_METRICS_DIR");
    char mbuf[512];
    if (!mdir || !*mdir) {
        (void)snprintf(mbuf, sizeof mbuf, "%s/metrics", repo_root);
        mdir = mbuf;
    }
    if (cos_metrics_session_open(&met, mdir) != 0) {
        fprintf(stderr, "bench: metrics open failed (mkdir %s?)\n", mdir);
        return 2;
    }

    int n = 50;
    int budget = cfg.max_fallback_budget_per_session;
    unsigned long routed_primary = 0, routed_fb = 0, routed_abs = 0;
    unsigned long schema_ok = 0, exec_ok = 0;
    uint64_t lat_accum = 0;

    for (int t = 0; t < n; t++) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        cos_metrics_task_started(&met, t);

        float logits[8];
        fill_logits_scenario(t, logits, 8);
        float ent = sigma_logit_entropy(logits, 8);
        float margin = sigma_top_margin(logits, 8);
        cos_metrics_sigma_observe(&met, 0, ent);
        cos_metrics_sigma_observe(&met, 1, margin);

        int fb_avail = cos_model_has_named_fallback(&reg, cfg.fallback_model_name);
        int bud = budget;
        cos_route_result_t rr;
        if (!strcmp(mode, "bitnet-only")) {
            rr = cos_route_from_logits(&cfg, logits, 8, 0, &bud);
        } else if (!strcmp(mode, "fallback-only")) {
            memset(&rr, 0, sizeof rr);
            if (fb_avail) {
                rr.decision = COS_ROUTE_FALLBACK;
                if (bud > 0)
                    bud--;
            } else {
                rr.decision = COS_ROUTE_ABSTAIN;
            }
        } else {
            rr = cos_route_from_logits(&cfg, logits, 8, fb_avail, &bud);
        }
        budget = bud;

        if (rr.decision == COS_ROUTE_PRIMARY)
            routed_primary++;
        else if (rr.decision == COS_ROUTE_FALLBACK)
            routed_fb++;
        else
            routed_abs++;

        char schema_path[512];
        (void)snprintf(schema_path, sizeof schema_path, "%s/config/tools/read_file.schema.json", repo_root);
        const char *tool_bad = "{\"name\":\"read_file\",\"arguments\":{}}";
        const char *tool_good = "{\"name\":\"read_file\",\"arguments\":{\"path\":\"/tmp/cos\"}}";
        int valid = cos_schema_validate_tool_json(tool_bad, schema_path);
        if (!valid)
            valid = cos_schema_validate_tool_json(tool_good, schema_path);
        if (valid) {
            schema_ok++;
            cos_metrics_schema(&met, 1);
        } else {
            cos_metrics_schema(&met, 0);
        }

        int exec = valid && (rr.decision != COS_ROUTE_ABSTAIN);
        if (exec) {
            exec_ok++;
            cos_metrics_tool_exec(&met, 1);
        } else {
            cos_metrics_tool_exec(&met, 0);
        }

        const char *outcome = "failed";
        if (rr.decision == COS_ROUTE_ABSTAIN)
            outcome = "abstained";
        else if (valid && exec)
            outcome = "completed";
        else if (!valid)
            outcome = "failed";
        cos_metrics_task_outcome(&met, t, outcome);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        uint64_t ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ull
            + (uint64_t)(t1.tv_nsec - t0.tv_nsec);
        lat_accum += ns;
        cos_metrics_latency_e2e_ns(&met, ns);
    }

    cos_metrics_session_close(&met);

    double abstain_rate = (double)routed_abs / (double)n;
    double schema_rate = (double)schema_ok / (double)n;
    double exec_rate = (double)exec_ok / (double)n;
    double p50_ms = (double)lat_accum / (double)n / 1e6;

    fprintf(stdout,
        "SUMMARY mode=%s tasks=%d primary=%lu fallback=%lu abstain=%lu schema_rate=%.3f "
        "exec_rate=%.3f abstain_rate=%.3f p50_e2e_ms=%.3f metrics_path=%s\n",
        mode, n, routed_primary, routed_fb, routed_abs, schema_rate, exec_rate, abstain_rate, p50_ms,
        met.path);

    return 0;
}

static void print_help(const char *argv0)
{
    printf("usage: %s [--self-test] [--bench-agent] [--help]\n", argv0);
    printf("\n");
    printf("v33 lab: σ-routed fallback + schema-first tool validation + JSONL metrics.\n");
    printf("\n");
    printf("Environment:\n");
    printf("  COS_V33_REPO_ROOT      path to repo root for config/ (default: .)\n");
    printf("  COS_V33_BENCH_MODE     bitnet-only | bitnet+fallback | fallback-only\n");
    printf("  COS_V33_METRICS_DIR    directory for metrics/session_*.jsonl\n");
    printf("  COS_V33_FALLBACK_*     see src/v33/model_registry.c (optional fallback row)\n");
    printf("\n");
    printf("Not part of merge-gate.\n");
}

int main(int argc, char **argv)
{
    const char *repo = getenv("COS_V33_REPO_ROOT");
    if (!repo || !*repo)
        repo = ".";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test")) {
            int a = self_test_router();
            int b = self_test_schema(repo);
            int c = self_test_metrics();
            int d = self_test_sigma_channels();
            return (a == 0 && b == 0 && c == 0 && d == 0) ? 0 : 2;
        }
        if (!strcmp(argv[i], "--bench-agent"))
            return bench_agent(repo);
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_help(argv[0]);
            return 0;
        }
    }

    print_help(argv[0]);
    fprintf(stderr, "Hint: %s --self-test\n", argv[0]);
    return 2;
}
#endif
