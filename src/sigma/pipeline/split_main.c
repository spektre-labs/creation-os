/* σ-Split self-test + canonical 3-node / 3-query demo.
 *
 * Pipeline: A (0..10) → B (11..20) → C (21..32).  τ_exit = 0.20,
 * min_stages = 0 (any confident stage may short-circuit).
 *   easy:   σ trace 0.10, 0.05, 0.03  → exit after A (B+C skipped)
 *   medium: σ trace 0.50, 0.15, 0.10  → exit after B (C skipped)
 *   hard:   σ trace 0.60, 0.40, 0.25  → full path, σ stays high
 */

#include "split.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int   cursor;
    const float *sig;
    const float *lat;
} script_t;

static int script_stage(int idx, int ls, int le, const char *nid,
                        void *vctx, float *out_sigma, float *out_lat)
{
    (void)idx; (void)ls; (void)le; (void)nid;
    script_t *s = (script_t*)vctx;
    *out_sigma = s->sig[s->cursor];
    *out_lat   = s->lat[s->cursor];
    s->cursor++;
    return 0;
}

static void run_one(const char *label,
                    cos_split_stage_t *stages, int n,
                    const float *sig, const float *lat,
                    cos_split_report_t *out)
{
    script_t s = { 0, sig, lat };
    cos_split_config_t cfg = {
        .tau_exit  = 0.20f,
        .min_stages = 0,
        .stage_fn  = script_stage,
        .ctx       = &s,
    };
    cos_sigma_split_run(&cfg, stages, n, out);
    (void)label;
}

int main(int argc, char **argv) {
    int rc = cos_sigma_split_self_test();

    cos_split_stage_t stages[3];
    int n = 0;
    cos_sigma_split_init(stages, 3, &n);
    cos_sigma_split_assign(stages, 3, &n, "node-A",  0, 10);
    cos_sigma_split_assign(stages, 3, &n, "node-B", 11, 20);
    cos_sigma_split_assign(stages, 3, &n, "node-C", 21, 32);

    float easy_s[3]   = { 0.10f, 0.05f, 0.03f };
    float easy_l[3]   = { 15.0f, 18.0f, 22.0f };
    float medium_s[3] = { 0.50f, 0.15f, 0.10f };
    float medium_l[3] = { 15.0f, 18.0f, 22.0f };
    float hard_s[3]   = { 0.60f, 0.40f, 0.25f };
    float hard_l[3]   = { 15.0f, 18.0f, 22.0f };

    cos_split_report_t easy, medium, hard;
    run_one("easy",   stages, n, easy_s,   easy_l,   &easy);
    run_one("medium", stages, n, medium_s, medium_l, &medium);
    run_one("hard",   stages, n, hard_s,   hard_l,   &hard);

    float full_latency   = 15.0f + 18.0f + 22.0f;
    float saved_easy     = full_latency - easy.total_latency_ms;
    float saved_medium   = full_latency - medium.total_latency_ms;

    printf("{\"kernel\":\"sigma_split\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"stages\":%d,"
             "\"tau_exit\":0.20,"
             "\"full_latency_ms\":%.2f,"
             "\"easy\":{\"stages_run\":%d,\"early_exit\":%s,"
                       "\"exit_stage\":%d,\"sigma\":%.2f,"
                       "\"latency_ms\":%.2f,\"saved_ms\":%.2f},"
             "\"medium\":{\"stages_run\":%d,\"early_exit\":%s,"
                         "\"exit_stage\":%d,\"sigma\":%.2f,"
                         "\"latency_ms\":%.2f,\"saved_ms\":%.2f},"
             "\"hard\":{\"stages_run\":%d,\"early_exit\":%s,"
                       "\"exit_stage\":%d,\"sigma\":%.2f,"
                       "\"latency_ms\":%.2f}"
             "},"
           "\"pass\":%s}\n",
           rc, n,
           (double)full_latency,
           easy.stages_run, easy.early_exit ? "true" : "false",
           easy.exit_stage_idx, (double)easy.final_sigma,
           (double)easy.total_latency_ms, (double)saved_easy,
           medium.stages_run, medium.early_exit ? "true" : "false",
           medium.exit_stage_idx, (double)medium.final_sigma,
           (double)medium.total_latency_ms, (double)saved_medium,
           hard.stages_run, hard.early_exit ? "true" : "false",
           hard.exit_stage_idx, (double)hard.final_sigma,
           (double)hard.total_latency_ms,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Split demo: easy→exit@%d hard→full σ=%.2f\n",
                easy.exit_stage_idx, (double)hard.final_sigma);
    }
    return rc;
}
