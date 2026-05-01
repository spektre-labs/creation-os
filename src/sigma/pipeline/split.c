/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Split — layer-partitioned inference with σ-gated early exit. */

#include "split.h"

#include <string.h>

int cos_sigma_split_init(cos_split_stage_t *stages, int cap, int *out_count) {
    if (!stages || cap <= 0) return -1;
    memset(stages, 0, sizeof(cos_split_stage_t) * (size_t)cap);
    if (out_count) *out_count = 0;
    return 0;
}

int cos_sigma_split_assign(cos_split_stage_t *stages, int cap, int *count,
                           const char *node_id, int layer_start, int layer_end)
{
    if (!stages || !count || !node_id) return -1;
    if (*count >= cap)                  return -2;
    if (layer_start < 0 || layer_end < layer_start) return -3;
    if (*count > 0) {
        int prev_end = stages[*count - 1].layer_end;
        if (layer_start != prev_end + 1) return -4; /* non-contiguous */
    } else {
        /* First stage must start at some non-negative layer; we do
         * not force 0 so a caller can prepend a cheap projection. */
    }
    cos_split_stage_t *s = &stages[*count];
    memset(s, 0, sizeof *s);
    s->stage_idx   = *count;
    s->layer_start = layer_start;
    s->layer_end   = layer_end;
    strncpy(s->node_id, node_id, COS_SPLIT_NODE_ID_CAP - 1);
    (*count)++;
    return 0;
}

int cos_sigma_split_run(const cos_split_config_t *cfg,
                        cos_split_stage_t *stages,
                        int n_stages,
                        cos_split_report_t *out)
{
    if (!cfg || !stages || !out || n_stages < 1 || !cfg->stage_fn) return -1;
    if (!(cfg->tau_exit > 0.0f && cfg->tau_exit <= 1.0f))          return -2;
    if (cfg->min_stages < 0)                                       return -3;

    memset(out, 0, sizeof *out);
    float acc_sigma   = 1.0f;   /* worst until proven otherwise   */
    float acc_latency = 0.0f;

    for (int i = 0; i < n_stages; ++i) {
        cos_split_stage_t *s = &stages[i];
        float sig = 1.0f, lat = 0.0f;
        int rc = cfg->stage_fn(i,
                               s->layer_start, s->layer_end,
                               s->node_id,
                               cfg->ctx,
                               &sig, &lat);
        if (rc != 0) {
            out->error            = 1;
            out->stages_run       = i;
            out->exit_stage_idx   = (i > 0) ? i - 1 : 0;
            out->final_sigma      = acc_sigma;
            out->total_latency_ms = acc_latency;
            return rc;
        }
        if (sig < 0.0f) sig = 0.0f;
        if (sig > 1.0f) sig = 1.0f;
        if (lat < 0.0f) lat = 0.0f;
        s->sigma_layer   = sig;
        s->latency_ms    = lat;
        s->executed      = 1;
        acc_sigma        = sig;          /* last σ wins: deeper
                                          * stages are trusted to
                                          * refine, not average.  */
        acc_latency     += lat;
        out->stages_run  = i + 1;
        out->exit_stage_idx   = i;
        out->final_sigma      = acc_sigma;
        out->total_latency_ms = acc_latency;

        /* Early exit only has meaning if there is at least one
         * downstream stage left to skip.  If the threshold trips
         * on the last stage, the pipeline already ran fully. */
        if (out->stages_run > cfg->min_stages &&
            acc_sigma < cfg->tau_exit &&
            i < n_stages - 1) {
            out->early_exit = 1;
            return 0;
        }
    }
    return 0;
}

/* ---------- self-test ---------- */

typedef struct {
    int   cursor;
    const float *sigma_trace;
    const float *latency_trace;
    int   n_trace;
    int   fail_at;             /* -1 = no failure                 */
} stub_ctx_t;

static int stub_stage(int idx, int ls, int le, const char *nid,
                      void *vctx, float *out_sigma, float *out_lat)
{
    (void)ls; (void)le; (void)nid;
    stub_ctx_t *c = (stub_ctx_t*)vctx;
    if (c->fail_at == idx) return 7;
    if (c->cursor >= c->n_trace) return 8;
    *out_sigma = c->sigma_trace  [c->cursor];
    *out_lat   = c->latency_trace[c->cursor];
    c->cursor++;
    return 0;
}

static int check_contiguity(void) {
    cos_split_stage_t stages[4];
    int n = 0;
    cos_sigma_split_init(stages, 4, &n);
    if (cos_sigma_split_assign(stages, 4, &n, "A", 0, 10) != 0)  return 10;
    if (cos_sigma_split_assign(stages, 4, &n, "B", 12, 20) != -4) return 11;
    if (cos_sigma_split_assign(stages, 4, &n, "B", 11, 20) != 0)  return 12;
    if (cos_sigma_split_assign(stages, 4, &n, "C", 21, 32) != 0)  return 13;
    if (n != 3)                                                    return 14;
    return 0;
}

static int check_full_path(void) {
    cos_split_stage_t stages[4];
    int n = 0;
    cos_sigma_split_init(stages, 4, &n);
    cos_sigma_split_assign(stages, 4, &n, "A", 0, 10);
    cos_sigma_split_assign(stages, 4, &n, "B", 11, 20);
    cos_sigma_split_assign(stages, 4, &n, "C", 21, 32);

    /* Hard query: σ stays above τ_exit=0.20 the whole way. */
    float sig[3] = { 0.60f, 0.40f, 0.15f };
    float lat[3] = { 15.0f, 18.0f, 22.0f };
    stub_ctx_t ctx = { 0, sig, lat, 3, -1 };
    cos_split_config_t cfg = {
        .tau_exit = 0.20f, .min_stages = 1,
        .stage_fn = stub_stage, .ctx = &ctx
    };
    cos_split_report_t rep;
    if (cos_sigma_split_run(&cfg, stages, n, &rep) != 0) return 20;
    if (rep.stages_run != 3)                             return 21;
    if (rep.early_exit)                                  return 22;
    if (rep.final_sigma != 0.15f)                        return 23;
    if (rep.total_latency_ms != 55.0f)                   return 24;
    return 0;
}

static int check_early_exit(void) {
    cos_split_stage_t stages[3];
    int n = 0;
    cos_sigma_split_init(stages, 3, &n);
    cos_sigma_split_assign(stages, 3, &n, "A", 0, 10);
    cos_sigma_split_assign(stages, 3, &n, "B", 11, 20);
    cos_sigma_split_assign(stages, 3, &n, "C", 21, 32);

    /* Easy query: σ already below τ_exit after stage 1 (index 1). */
    float sig[3] = { 0.50f, 0.10f, 0.05f };
    float lat[3] = { 15.0f, 18.0f, 22.0f };
    stub_ctx_t ctx = { 0, sig, lat, 3, -1 };
    cos_split_config_t cfg = {
        .tau_exit = 0.20f, .min_stages = 1,
        .stage_fn = stub_stage, .ctx = &ctx
    };
    cos_split_report_t rep;
    if (cos_sigma_split_run(&cfg, stages, n, &rep) != 0) return 30;
    if (!rep.early_exit)                                  return 31;
    if (rep.stages_run != 2)                              return 32;
    if (rep.exit_stage_idx != 1)                          return 33;
    if (rep.final_sigma != 0.10f)                         return 34;
    /* Only A + B wall time accumulated.                    */
    if (rep.total_latency_ms != 33.0f)                    return 35;
    /* C must not have run.                                 */
    if (stages[2].executed != 0)                          return 36;
    return 0;
}

static int check_min_stages_guard(void) {
    cos_split_stage_t stages[3];
    int n = 0;
    cos_sigma_split_init(stages, 3, &n);
    cos_sigma_split_assign(stages, 3, &n, "A", 0, 10);
    cos_sigma_split_assign(stages, 3, &n, "B", 11, 20);
    cos_sigma_split_assign(stages, 3, &n, "C", 21, 32);

    /* Even though stage 0 is already confident, min_stages=2
     * forces us to complete at least 2 before early exit may
     * fire.  So we should exit after stage 1, not 0.           */
    float sig[3] = { 0.05f, 0.04f, 0.03f };
    float lat[3] = { 10.0f, 10.0f, 10.0f };
    stub_ctx_t ctx = { 0, sig, lat, 3, -1 };
    cos_split_config_t cfg = {
        .tau_exit = 0.20f, .min_stages = 2,
        .stage_fn = stub_stage, .ctx = &ctx
    };
    cos_split_report_t rep;
    if (cos_sigma_split_run(&cfg, stages, n, &rep) != 0) return 40;
    /* stages_run must be exactly 3 — min_stages=2 means we need
     * >2 completed before early exit; only stage index 2 qualifies,
     * which is already the last stage so no time saved.         */
    if (rep.stages_run != 3)                              return 41;
    return 0;
}

static int check_transport_error(void) {
    cos_split_stage_t stages[3];
    int n = 0;
    cos_sigma_split_init(stages, 3, &n);
    cos_sigma_split_assign(stages, 3, &n, "A", 0, 10);
    cos_sigma_split_assign(stages, 3, &n, "B", 11, 20);
    cos_sigma_split_assign(stages, 3, &n, "C", 21, 32);

    float sig[3] = { 0.50f, 0.40f, 0.15f };
    float lat[3] = { 15.0f, 18.0f, 22.0f };
    stub_ctx_t ctx = { 0, sig, lat, 3, 1 /* fail on stage 1 */ };
    cos_split_config_t cfg = {
        .tau_exit = 0.20f, .min_stages = 1,
        .stage_fn = stub_stage, .ctx = &ctx
    };
    cos_split_report_t rep;
    int rc = cos_sigma_split_run(&cfg, stages, n, &rep);
    if (rc != 7)          return 50;
    if (!rep.error)       return 51;
    if (rep.stages_run != 1) return 52;
    return 0;
}

int cos_sigma_split_self_test(void) {
    int rc;
    if ((rc = check_contiguity()))       return rc;
    if ((rc = check_full_path()))        return rc;
    if ((rc = check_early_exit()))       return rc;
    if ((rc = check_min_stages_guard())) return rc;
    if ((rc = check_transport_error()))  return rc;
    return 0;
}
