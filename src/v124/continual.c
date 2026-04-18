/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v124 σ-Continual — implementation (pure C, weights-free).
 *
 * What this file is: the *policy + buffer + forgetting-smoke harness*
 * for idle-time continual learning.  The actual MLX LoRA trainer and
 * the hot-swap into v106 land in v124.1 — see docs/v124/README.md.
 *
 * What this file is not: a trainer.  `cos_v124_run_epoch` simulates
 * a successful micro-tune by advancing the adapter version; the
 * baseline accuracy is supplied by a caller-provided callback.  The
 * self-test provides two callbacks — a healthy one that keeps
 * accuracy stable, and a pathological one that drops 4 % on the
 * 10th epoch — to exercise both the happy path and the rollback.
 */
#include "continual.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================
 * Config + helpers
 * ================================================================== */

void cos_v124_config_defaults(cos_v124_config_t *cfg) {
    if (!cfg) return;
    cfg->idle_threshold_sec    = COS_V124_IDLE_SEC_DEFAULT;
    cfg->training_trigger_size = COS_V124_TRIGGER_SIZE_DEFAULT;
    cfg->tau_high              = COS_V124_TAU_HIGH_DEFAULT;
    cfg->tau_anchor            = COS_V124_TAU_ANCHOR_DEFAULT;
    cfg->lora_steps            = COS_V124_LORA_STEPS_DEFAULT;
    cfg->smoke_every_n_epochs  = COS_V124_SMOKE_EVERY_DEFAULT;
    cfg->max_baseline_drop     = COS_V124_MAX_BASELINE_DROP_DEF;
    cfg->smoke_baseline_n      = COS_V124_SMOKE_BASELINE_N_DEF;
    cfg->buffer_capacity       = 100;
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Minimal safe JSON string escaper (escapes \" and \\ and control). */
static size_t json_escape(const char *s, char *out, size_t cap) {
    size_t n = 0;
    if (!out || cap == 0) return 0;
    for (const unsigned char *p = (const unsigned char *)s; p && *p; ++p) {
        const char *esc = NULL;
        char        buf[8];
        switch (*p) {
            case '"':  esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n";  break;
            case '\r': esc = "\\r";  break;
            case '\t': esc = "\\t";  break;
            default:
                if (*p < 0x20) {
                    snprintf(buf, sizeof buf, "\\u%04x", *p);
                    esc = buf;
                }
                break;
        }
        if (esc) {
            size_t k = strlen(esc);
            if (n + k + 1 >= cap) break;
            memcpy(out + n, esc, k);
            n += k;
        } else {
            if (n + 1 + 1 >= cap) break;
            out[n++] = (char)*p;
        }
    }
    out[n] = 0;
    return n;
}

/* ====================================================================
 * Ring buffer
 * ================================================================== */

void cos_v124_buffer_init(cos_v124_buffer_t *buf, int capacity) {
    if (!buf) return;
    memset(buf, 0, sizeof *buf);
    buf->capacity = clampi(capacity > 0 ? capacity : 100,
                           1, COS_V124_BUFFER_MAX);
}

int cos_v124_buffer_size(const cos_v124_buffer_t *buf) {
    return buf ? buf->size : 0;
}

void cos_v124_buffer_append(cos_v124_buffer_t *buf,
                            const cos_v124_interaction_t *it) {
    if (!buf || !it) return;
    int cap = buf->capacity > 0 ? buf->capacity : 1;
    buf->items[buf->head] = *it;
    buf->head = (buf->head + 1) % cap;
    if (buf->size < cap) buf->size++;
}

static int buf_index(const cos_v124_buffer_t *buf, int logical) {
    /* logical 0 = oldest; logical size-1 = newest. */
    int cap = buf->capacity;
    int oldest = (buf->size < cap) ? 0 : buf->head;
    return (oldest + logical) % cap;
}

int cos_v124_buffer_select(const cos_v124_config_t *cfg,
                           const cos_v124_buffer_t *buf,
                           cos_v124_training_batch_t *out) {
    if (!cfg || !buf || !out) return 0;
    memset(out, 0, sizeof *out);
    for (int i = 0; i < buf->size; ++i) {
        const cos_v124_interaction_t *it =
            &buf->items[buf_index(buf, i)];
        int counted = 0;

        if (it->user_corrected && it->correction[0]) {
            out->n_preferences++;
            counted = 1;
        }
        if (!counted && it->sigma_product > cfg->tau_high) {
            out->n_high_sigma++;
            counted = 1;
        }
        if (!counted && it->sigma_product < cfg->tau_anchor) {
            /* anchor: low-σ, known-good, keeps weights from drifting */
            out->n_anchors++;
            counted = 1;
        }
        /* mid-σ rows are skipped: neither teachable nor anchor */
    }
    out->n_total = out->n_high_sigma + out->n_anchors + out->n_preferences;
    return out->n_total;
}

int cos_v124_buffer_write_jsonl(const cos_v124_buffer_t *buf, FILE *fp) {
    if (!buf || !fp) return -1;
    int wrote = 0;
    char pe[COS_V124_MAX_FIELD * 2 + 16];
    char re[COS_V124_MAX_FIELD * 2 + 16];
    char ce[COS_V124_MAX_FIELD * 2 + 16];
    for (int i = 0; i < buf->size; ++i) {
        const cos_v124_interaction_t *it =
            &buf->items[buf_index(buf, i)];
        json_escape(it->prompt,     pe, sizeof pe);
        json_escape(it->response,   re, sizeof re);
        json_escape(it->correction, ce, sizeof ce);
        int n = fprintf(fp,
            "{\"ts\":%llu,\"prompt\":\"%s\",\"response\":\"%s\","
            "\"sigma_product\":%.4f,\"corrected\":%d,\"correction\":\"%s\"}\n",
            (unsigned long long)it->ts_unix, pe, re,
            (double)it->sigma_product,
            it->user_corrected ? 1 : 0, ce);
        if (n < 0) return -1;
        wrote++;
    }
    return wrote;
}

/* ====================================================================
 * Trigger policy
 * ================================================================== */

cos_v124_trigger_t
cos_v124_trigger_decide(const cos_v124_config_t *cfg,
                        uint64_t idle_seconds,
                        int buffer_size,
                        int next_epoch_id) {
    if (!cfg) return COS_V124_TRIGGER_SKIP;
    if (idle_seconds < cfg->idle_threshold_sec)
        return COS_V124_TRIGGER_SKIP;
    if (buffer_size < cfg->training_trigger_size)
        return COS_V124_TRIGGER_SKIP;
    if (cfg->smoke_every_n_epochs > 0 &&
        next_epoch_id > 0 &&
        (next_epoch_id % cfg->smoke_every_n_epochs) == 0)
        return COS_V124_TRIGGER_SMOKE;
    return COS_V124_TRIGGER_TRAIN;
}

/* ====================================================================
 * Epoch execution — synthetic micro-tune + optional smoke
 * ================================================================== */

int cos_v124_run_epoch(const cos_v124_config_t *cfg,
                       cos_v124_buffer_t       *buf,
                       uint32_t                 prev_adapter_version,
                       float                    prev_baseline_accuracy,
                       cos_v124_baseline_fn     baseline_fn,
                       void                    *baseline_ctx,
                       cos_v124_epoch_t        *out) {
    if (!cfg || !buf || !out) return -1;
    memset(out, 0, sizeof *out);

    cos_v124_training_batch_t batch;
    int n = cos_v124_buffer_select(cfg, buf, &batch);
    out->epoch_id                = prev_adapter_version + 1;
    out->batch                   = batch;
    out->lora_steps_ran          = (n > 0) ? cfg->lora_steps : 0;
    out->baseline_accuracy_before = prev_baseline_accuracy;
    out->baseline_accuracy_after  = prev_baseline_accuracy;
    out->active_adapter_version   = out->epoch_id;

    if (n == 0) {
        /* Nothing to train — treat as a no-op; keep previous adapter. */
        out->active_adapter_version = prev_adapter_version;
        return 0;
    }

    /* Optional forgetting smoke: every Nth epoch. */
    int smoke_due = (cfg->smoke_every_n_epochs > 0 &&
                     (int)out->epoch_id > 0 &&
                     ((int)out->epoch_id % cfg->smoke_every_n_epochs) == 0);
    if (smoke_due && baseline_fn) {
        out->smoke_ran = 1;
        float acc = baseline_fn(out->active_adapter_version,
                                cfg->smoke_baseline_n,
                                baseline_ctx);
        out->baseline_accuracy_after = acc;
        float drop = prev_baseline_accuracy - acc;
        if (prev_baseline_accuracy > 0.0f && drop > cfg->max_baseline_drop) {
            /* Catastrophic forgetting prevented — rollback. */
            out->forgetting_detected    = 1;
            out->rolled_back            = 1;
            out->active_adapter_version = prev_adapter_version;
        }
    }

    /* Drain the buffer — caller may persist it separately before
     * calling us. */
    buf->size = 0;
    buf->head = 0;
    return 0;
}

/* ====================================================================
 * JSON serialization
 * ================================================================== */

int cos_v124_epoch_to_json(const cos_v124_epoch_t *e,
                           char *out, size_t cap) {
    if (!e || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"epoch_id\":%u,"
        "\"batch\":{\"n_high_sigma\":%d,\"n_anchors\":%d,"
        "\"n_preferences\":%d,\"n_total\":%d},"
        "\"lora_steps_ran\":%d,"
        "\"smoke_ran\":%d,"
        "\"baseline_accuracy_before\":%.4f,"
        "\"baseline_accuracy_after\":%.4f,"
        "\"forgetting_detected\":%d,"
        "\"rolled_back\":%d,"
        "\"active_adapter_version\":%u}",
        e->epoch_id,
        e->batch.n_high_sigma, e->batch.n_anchors,
        e->batch.n_preferences, e->batch.n_total,
        e->lora_steps_ran,
        e->smoke_ran ? 1 : 0,
        (double)e->baseline_accuracy_before,
        (double)e->baseline_accuracy_after,
        e->forgetting_detected ? 1 : 0,
        e->rolled_back ? 1 : 0,
        e->active_adapter_version);
}

int cos_v124_stats_to_json(const cos_v124_stats_t *s,
                           char *out, size_t cap) {
    if (!s || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"epochs_completed\":%d,"
        "\"epochs_with_smoke\":%d,"
        "\"epochs_rolled_back\":%d,"
        "\"last_baseline_accuracy\":%.4f,"
        "\"active_adapter_version\":%u}",
        s->epochs_completed,
        s->epochs_with_smoke,
        s->epochs_rolled_back,
        (double)s->last_baseline_accuracy,
        s->active_adapter_version);
}

/* ====================================================================
 * Self-test
 * ================================================================== */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v124 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

/* Healthy baseline: always 0.92, no drop. */
static float healthy_baseline(uint32_t adapter, int n, void *ctx) {
    (void)adapter; (void)n; (void)ctx;
    return 0.92f;
}

/* Pathological baseline: fine until epoch 10, then -4 %. */
static float catastrophic_baseline(uint32_t adapter, int n, void *ctx) {
    (void)n; (void)ctx;
    if (adapter == 10) return 0.88f;  /* 4 % drop against 0.92 */
    return 0.92f;
}

static void make_interaction(cos_v124_interaction_t *it,
                             uint64_t ts, const char *prompt,
                             const char *response, float sigma,
                             int corrected, const char *correction) {
    memset(it, 0, sizeof *it);
    it->ts_unix        = ts;
    it->sigma_product  = sigma;
    it->user_corrected = corrected;
    snprintf(it->prompt,     sizeof it->prompt,     "%s", prompt);
    snprintf(it->response,   sizeof it->response,   "%s", response);
    if (correction)
        snprintf(it->correction, sizeof it->correction, "%s", correction);
}

int cos_v124_self_test(void) {
    cos_v124_config_t cfg;
    cos_v124_config_defaults(&cfg);

    /* --- Buffer: ring-mode semantics ------------------------------- */
    fprintf(stderr, "check-v124: ring buffer wrap\n");
    cos_v124_buffer_t buf;
    cos_v124_buffer_init(&buf, 4);
    cos_v124_interaction_t it;
    for (int i = 0; i < 7; ++i) {
        char p[64], r[64];
        snprintf(p, sizeof p, "prompt-%d", i);
        snprintf(r, sizeof r, "resp-%d",  i);
        make_interaction(&it, 1700000000ULL + i, p, r,
                         (i % 2) ? 0.80f : 0.20f, 0, NULL);
        cos_v124_buffer_append(&buf, &it);
    }
    _CHECK(buf.size == 4, "buffer size should saturate at capacity");
    /* Oldest surviving should be prompt-3 (we wrote 0..6). */
    int oldest = buf_index(&buf, 0);
    _CHECK(strcmp(buf.items[oldest].prompt, "prompt-3") == 0,
           "ring buffer oldest should be prompt-3 after 7 writes cap=4");

    /* --- Selection: high-σ vs anchor vs preferences ---------------- */
    fprintf(stderr, "check-v124: σ-targeted batch selection\n");
    cos_v124_buffer_init(&buf, 16);
    make_interaction(&it, 1, "capital of france?",      "paris",    0.15f, 0, NULL);
    cos_v124_buffer_append(&buf, &it);
    make_interaction(&it, 2, "capital of estonia?",     "maybe..",  0.82f, 0, NULL);
    cos_v124_buffer_append(&buf, &it);
    make_interaction(&it, 3, "how many legs has ant?",  "4",        0.90f, 1, "6");
    cos_v124_buffer_append(&buf, &it);
    make_interaction(&it, 4, "2+2?",                    "4",        0.05f, 0, NULL);
    cos_v124_buffer_append(&buf, &it);
    make_interaction(&it, 5, "mid-conf",                "meh",      0.45f, 0, NULL);
    cos_v124_buffer_append(&buf, &it);
    cos_v124_training_batch_t batch;
    int n = cos_v124_buffer_select(&cfg, &buf, &batch);
    _CHECK(n == 4,                         "4 of 5 rows should be selected");
    _CHECK(batch.n_high_sigma  == 1,       "exactly one high-σ row (τ=0.60)");
    _CHECK(batch.n_anchors     == 2,       "two anchors (σ<0.30)");
    _CHECK(batch.n_preferences == 1,       "one preference pair");

    /* --- Trigger policy -------------------------------------------- */
    fprintf(stderr, "check-v124: idle-trigger policy\n");
    _CHECK(cos_v124_trigger_decide(&cfg, 30, 20, 1) == COS_V124_TRIGGER_SKIP,
           "skip when idle<60s");
    _CHECK(cos_v124_trigger_decide(&cfg, 90, 2, 1) == COS_V124_TRIGGER_SKIP,
           "skip when buffer too small");
    _CHECK(cos_v124_trigger_decide(&cfg, 90, 20, 1) == COS_V124_TRIGGER_TRAIN,
           "train when idle + buffer ready");
    _CHECK(cos_v124_trigger_decide(&cfg, 90, 20, 10) == COS_V124_TRIGGER_SMOKE,
           "smoke every 10th epoch");

    /* --- Healthy epoch: no forgetting ------------------------------ */
    fprintf(stderr, "check-v124: healthy epoch (no forgetting)\n");
    cos_v124_buffer_init(&buf, 100);
    for (int i = 0; i < 10; ++i) {
        char p[32], r[32];
        snprintf(p, sizeof p, "q-%d", i);
        snprintf(r, sizeof r, "a-%d", i);
        make_interaction(&it, 100 + i, p, r,
                         (i < 5) ? 0.72f : 0.20f, 0, NULL);
        cos_v124_buffer_append(&buf, &it);
    }
    cos_v124_epoch_t e;
    int rc = cos_v124_run_epoch(&cfg, &buf, 9u /* prev adapter */,
                                0.92f /* prev baseline */,
                                healthy_baseline, NULL, &e);
    _CHECK(rc == 0,                              "run_epoch healthy rc");
    _CHECK(e.epoch_id == 10,                     "epoch_id increments");
    _CHECK(e.smoke_ran == 1,                     "epoch 10 runs smoke");
    _CHECK(e.forgetting_detected == 0,           "healthy: no forgetting");
    _CHECK(e.rolled_back == 0,                   "healthy: no rollback");
    _CHECK(e.active_adapter_version == 10,       "healthy: keep new adapter");
    _CHECK(e.batch.n_total > 0,                  "batch non-empty");

    /* --- Pathological epoch: rollback ------------------------------ */
    fprintf(stderr, "check-v124: catastrophic forgetting → rollback\n");
    cos_v124_buffer_init(&buf, 100);
    for (int i = 0; i < 10; ++i) {
        char p[32], r[32];
        snprintf(p, sizeof p, "q-%d", i);
        snprintf(r, sizeof r, "a-%d", i);
        make_interaction(&it, 200 + i, p, r, 0.72f, 0, NULL);
        cos_v124_buffer_append(&buf, &it);
    }
    rc = cos_v124_run_epoch(&cfg, &buf, 9u, 0.92f,
                            catastrophic_baseline, NULL, &e);
    _CHECK(rc == 0,                              "pathological rc");
    _CHECK(e.smoke_ran == 1,                     "pathological runs smoke");
    _CHECK(e.forgetting_detected == 1,           "forgetting detected");
    _CHECK(e.rolled_back == 1,                   "rollback triggered");
    _CHECK(e.active_adapter_version == 9,        "rollback keeps prev adapter");
    _CHECK(fabsf(e.baseline_accuracy_after - 0.88f) < 1e-4f,
                                                 "recorded post-smoke accuracy");

    /* --- JSON shape ------------------------------------------------- */
    fprintf(stderr, "check-v124: JSON serialization shape\n");
    char js[512];
    int w = cos_v124_epoch_to_json(&e, js, sizeof js);
    _CHECK(w > 0 && w < (int)sizeof js,          "epoch JSON wrote");
    _CHECK(strstr(js, "\"rolled_back\":1") != NULL, "rolled_back in JSON");
    _CHECK(strstr(js, "\"forgetting_detected\":1") != NULL,
                                                 "forgetting_detected in JSON");

    cos_v124_stats_t s = {
        .epochs_completed       = 10,
        .epochs_with_smoke      = 1,
        .epochs_rolled_back     = 1,
        .last_baseline_accuracy = 0.88f,
        .active_adapter_version = 9,
    };
    w = cos_v124_stats_to_json(&s, js, sizeof js);
    _CHECK(w > 0 && w < (int)sizeof js,          "stats JSON wrote");
    _CHECK(strstr(js, "\"epochs_rolled_back\":1") != NULL, "stats shape");

    fprintf(stderr, "check-v124: OK (buffer + trigger + healthy + rollback + JSON)\n");
    return 0;
}
