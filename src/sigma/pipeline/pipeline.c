/* sigma_pipeline — end-to-end composition of the σ-primitives.
 *
 * See pipeline.h for contracts.  This file is the orchestrator:
 * every decision is a call into an existing σ-primitive; text
 * generation is injected via a callback so integration tests can
 * drive every branch deterministically.
 */

#include "pipeline.h"
#include "ttt_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Canonical abstain message when the model does not know and the
 * sovereign mode forbids cloud escalation.  Deliberately short so
 * `cos chat` can render it inline. */
static const char *ABSTAIN_TEXT =
    "I am uncertain and cannot answer this locally "
    "(σ above threshold; escalation not permitted).";

/* Forward decls of helpers defined below. */
static void note_diagnostic(cos_pipeline_result_t *r,
                            const char *msg);
static const char *strdup_safe(const char *s);

/* -------- config defaults -------- */

void cos_sigma_pipeline_config_defaults(cos_pipeline_config_t *cfg) {
    if (cfg == NULL) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->tau_accept   = 0.20f;
    cfg->tau_rethink  = 0.60f;
    cfg->max_rethink  = COS_SIGMA_REINFORCE_MAX_ROUNDS;
    cfg->mode         = COS_PIPELINE_MODE_HYBRID;
    cfg->ttt_enabled  = 0;
}

/* -------- engram helpers -------- */

/* Look up input in the engram.  Returns 1 on HIT (fills *out_entry),
 * 0 on MISS, negative on error. */
static int pipeline_engram_lookup(cos_pipeline_config_t *cfg,
                                  const char *input,
                                  cos_sigma_engram_entry_t *out_entry) {
    if (cfg->engram == NULL || input == NULL) return 0;
    uint64_t h = cos_sigma_engram_hash(input);
    return cos_sigma_engram_get(cfg->engram, h, out_entry);
}

/* Store the (input, response, sigma) triple in the engram.
 * Duplicates the response text with malloc; on eviction the caller
 * must free the old value via cos_sigma_pipeline_free_engram_values
 * or a custom on_evict callback. */
static void pipeline_engram_store(cos_pipeline_config_t *cfg,
                                  const char *input,
                                  const char *response,
                                  float sigma) {
    if (cfg->engram == NULL || input == NULL || response == NULL) return;
    uint64_t h = cos_sigma_engram_hash(input);

    /* Free any previous value for this key before overwriting.
     * Also record whether the pre-put state was empty so we can
     * tell "new insert" apart from "overwrite" for the persistence
     * hook (both paths still write-through, but the hook is free
     * to treat them differently if it wants). */
    cos_sigma_engram_entry_t existing;
    int had_prior = (cos_sigma_engram_get(cfg->engram, h, &existing) == 1);
    if (had_prior && existing.value != NULL) {
        free(existing.value);
    }

    char *copy = (char *)strdup_safe(response);
    if (copy == NULL) return;
    uint64_t evicted = 0;
    if (cos_sigma_engram_put(cfg->engram, h, sigma, copy, &evicted) == 1) {
        /* Full table → one slot was evicted; we don't know which
         * value pointer it was (engram only surfaces the evicted
         * key hash, not the pointer).  In practice the caller
         * drains the whole table at shutdown via
         * cos_sigma_pipeline_free_engram_values(). */
        (void)evicted;
    }
    /* DEV-3: fan out to the persistence hook.  Runs AFTER put()
     * succeeds so the hook sees a value that is already in the
     * in-memory cache.  Callers may use this to write-through to
     * SQLite (~/.cos/engram.db).  `had_prior` is informational —
     * most implementations will just UPSERT unconditionally. */
    (void)had_prior;
    if (cfg->on_engram_store != NULL) {
        cfg->on_engram_store(input, h, response, sigma,
                             cfg->on_engram_store_ctx);
    }
}

/* -------- sovereign helpers -------- */

static void pipeline_track_local(cos_pipeline_config_t *cfg, double eur) {
    if (cfg->sovereign != NULL) {
        cos_sigma_sovereign_record_local(cfg->sovereign, eur);
    }
}

static void pipeline_track_cloud(cos_pipeline_config_t *cfg, double eur,
                                 uint64_t sent, uint64_t recv) {
    if (cfg->sovereign != NULL) {
        cos_sigma_sovereign_record_cloud(cfg->sovereign, eur, sent, recv);
    }
}

static void pipeline_track_abstain(cos_pipeline_config_t *cfg) {
    if (cfg->sovereign != NULL) {
        cos_sigma_sovereign_record_abstain(cfg->sovereign);
    }
}

/* -------- main entry -------- */

int cos_sigma_pipeline_run(cos_pipeline_config_t *cfg,
                           const char *input,
                           cos_pipeline_result_t *out) {
    if (cfg == NULL || out == NULL) return -1;
    if (cfg->generate == NULL) return -2;
    if (input == NULL) return -3;

    /* Reset result. */
    memset(out, 0, sizeof(*out));
    out->mode         = cfg->mode;
    out->sigma        = 1.0f;
    out->final_action = COS_SIGMA_ACTION_ABSTAIN;
    out->agent_gate   = COS_AGENT_BLOCK;
    out->response     = ABSTAIN_TEXT;
    out->codex_hash   = (cfg->codex != NULL) ? cfg->codex->hash_fnv1a64 : 0;
    out->ttt_applied  = 0;

    if (cfg->ttt_enabled)
        cos_ttt_turn_begin();

    /* [3] Engram lookup. */
    cos_sigma_engram_entry_t hit;
    if (pipeline_engram_lookup(cfg, input, &hit) == 1) {
        /* HIT: σ-at-store is the trust signal.  If it is below
         * τ_accept we short-circuit; otherwise we fall through
         * and regenerate (the cache entry was stored when the
         * model was uncertain). */
        if (hit.sigma_at_store < cfg->tau_accept && hit.value != NULL) {
            out->engram_hit    = true;
            out->response      = (const char *)hit.value;
            out->sigma         = hit.sigma_at_store;
            out->final_action  = COS_SIGMA_ACTION_ACCEPT;
            out->cost_eur      = 0.0;
            out->agent_gate    = COS_AGENT_ALLOW;
            note_diagnostic(out, "engram-hit");
            pipeline_track_local(cfg, 0.0);
            return 0;
        }
    }

    /* [4] First generation attempt. */
    const char *text = NULL;
    float       sigma = 1.0f;
    double      cost  = 0.0;
    int rc = cfg->generate(input, 0, cfg->generate_ctx,
                           &text, &sigma, &cost);
    if (rc != 0 || text == NULL) {
        note_diagnostic(out, "generate-failed");
        out->final_action = COS_SIGMA_ACTION_ABSTAIN;
        pipeline_track_abstain(cfg);
        return 0;  /* logically successful turn, just an abstain. */
    }
    out->response = text;
    out->sigma    = sigma;
    out->cost_eur = cost;
    pipeline_track_local(cfg, cost);

    float first_sigma = sigma;

    /* [7] σ-gate decision. */
    cos_sigma_action_t action =
        cos_sigma_reinforce(sigma, cfg->tau_accept, cfg->tau_rethink);

    /* [8] RETHINK loop (DEV-2: real regeneration).
     *
     * Policy: regenerate whenever the σ-gate does not say ACCEPT and
     * we still have budget.  That means BOTH the middle-band
     * (τ_accept ≤ σ < τ_rethink, classic RETHINK) AND the high-σ
     * tail (σ ≥ τ_rethink) get a second chance at the local model
     * before we fall through to escalation or ABSTAIN.  A single
     * bad sampler trajectory should not short-circuit the gate —
     * the caller's `generate(..., round, ...)` rotates seed +
     * temperature each round, so subsequent rounds explore a
     * genuinely different trajectory.
     *
     * Budget-exhausted state: if σ is still ≥ τ_accept after the
     * final round, resolve per threshold:
     *   σ ≥ τ_rethink → ABSTAIN (always).
     *   middle-band   → ABSTAIN (same as classic reinforce_round
     *                   MAX_ROUNDS-1 hard-fall).  */
    int round = 0;
    int budget = cfg->max_rethink > 0 ? cfg->max_rethink
                                      : COS_SIGMA_REINFORCE_MAX_ROUNDS;
    while (action != COS_SIGMA_ACTION_ACCEPT && round < budget - 1) {
        round++;
        if (cfg->ttt_enabled && round >= 1)
            cos_ttt_prepare_rethink(input, out->response, out->sigma);
        rc = cfg->generate(input, round, cfg->generate_ctx,
                           &text, &sigma, &cost);
        if (rc != 0 || text == NULL) {
            note_diagnostic(out, "rethink-generate-failed");
            action = COS_SIGMA_ACTION_ABSTAIN;
            break;
        }
        if (cfg->ttt_enabled && round >= 1
            && cos_ttt_should_revert(sigma))
            cos_ttt_revert(NULL);
        out->response   = text;
        out->sigma      = sigma;
        out->cost_eur  += cost;
        pipeline_track_local(cfg, cost);
        action = cos_sigma_reinforce(
            sigma, cfg->tau_accept, cfg->tau_rethink);
    }
    /* Budget-exhausted RETHINK → ABSTAIN (escalate / hold the line). */
    if (action == COS_SIGMA_ACTION_RETHINK) {
        action = COS_SIGMA_ACTION_ABSTAIN;
    }
    out->rethink_count = round;

    if (cfg->ttt_enabled && round > 0 && cos_ttt_episode_applied_flag()
        && !cos_ttt_was_reverted() && out->sigma < first_sigma - 1e-5f)
        out->ttt_applied = 1;

    /* [9] Escalation. */
    if (action == COS_SIGMA_ACTION_ABSTAIN
        && cfg->mode != COS_PIPELINE_MODE_LOCAL_ONLY
        && cfg->escalate != NULL) {
        const char *cloud_text = NULL;
        float cloud_sigma = 1.0f;
        double cloud_cost = 0.0;
        uint64_t sent = 0, recv = 0;
        int ecc = cfg->escalate(input, cfg->escalate_ctx,
                                &cloud_text, &cloud_sigma, &cloud_cost,
                                &sent, &recv);
        if (ecc == 0 && cloud_text != NULL) {
            out->response   = cloud_text;
            out->sigma      = cloud_sigma;
            out->cost_eur  += cloud_cost;
            out->escalated  = true;
            pipeline_track_cloud(cfg, cloud_cost, sent, recv);
            /* Escalation is treated as "answer delivered" → ACCEPT
             * for downstream state, even if σ is still elevated. */
            action = COS_SIGMA_ACTION_ACCEPT;
            note_diagnostic(out, "escalated");
        } else {
            note_diagnostic(out, "escalation-failed");
            pipeline_track_abstain(cfg);
        }
    } else if (action == COS_SIGMA_ACTION_ABSTAIN) {
        /* LOCAL_ONLY or no escalator — hold the line. */
        out->response = ABSTAIN_TEXT;
        note_diagnostic(out, "abstain-local-only");
        pipeline_track_abstain(cfg);
    }

    /* [10] Agent autonomy gate (optional).  WRITE is the default
     * class for "model emitted a response"; callers can run a
     * second gate on higher classes (NETWORK, IRREVERSIBLE) from
     * the CLI layer. */
    if (cfg->agent != NULL) {
        out->agent_gate = cos_sigma_agent_gate(
            cfg->agent, COS_ACT_WRITE, out->sigma);
    } else {
        /* No agent → treat as ALLOW by default (the caller already
         * opted into an "answer immediately" flow). */
        out->agent_gate = COS_AGENT_ALLOW;
    }

    out->final_action = action;

    /* [13] Engram store.  Only cache on ACCEPT (or post-escalation
     * ACCEPT) with a reasonable σ; do NOT poison the cache with
     * abstain-text entries. */
    if (action == COS_SIGMA_ACTION_ACCEPT && !out->engram_hit) {
        pipeline_engram_store(cfg, input, out->response, out->sigma);
    }

    /* [11] Diagnostic — if nothing else was said, pin the action. */
    if (out->diagnostic == NULL) {
        note_diagnostic(out, cos_sigma_action_label(action));
    }

    return 0;
}

/* -------- helpers -------- */

static void note_diagnostic(cos_pipeline_result_t *r, const char *msg) {
    /* Pointer into a small static pool so the lifetime is trivial. */
    static const char *const POOL[] = {
        "ACCEPT", "RETHINK", "ABSTAIN",
        "engram-hit", "escalated", "abstain-local-only",
        "generate-failed", "rethink-generate-failed",
        "escalation-failed",
    };
    for (size_t i = 0; i < sizeof(POOL) / sizeof(POOL[0]); ++i) {
        if (strcmp(POOL[i], msg) == 0) { r->diagnostic = POOL[i]; return; }
    }
    /* Fallback: the caller handed us a literal with static lifetime. */
    r->diagnostic = msg;
}

static const char *strdup_safe(const char *s) {
    if (s == NULL) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p == NULL) return NULL;
    memcpy(p, s, n);
    return p;
}

/* Public helper: free every value pointer currently stored in the
 * engram.  Use at shutdown before the engram slots array itself is
 * released.  Complements cos_sigma_engram_age_sweep (which frees
 * only expired entries). */
void cos_sigma_pipeline_free_engram_values(cos_sigma_engram_t *e) {
    if (e == NULL || e->slots == NULL) return;
    for (uint32_t i = 0; i < e->cap; ++i) {
        if (e->slots[i].key_hash != 0 && e->slots[i].value != NULL) {
            free(e->slots[i].value);
            e->slots[i].value = NULL;
        }
    }
}

/* -------- self-test stub generator -------- */

/* The self-test needs a deterministic generator so every branch is
 * covered in-process.  Scenarios are keyed by prompt prefix:
 *
 *   "low:"      → always σ = 0.05 → ACCEPT first try
 *   "high:"     → always σ = 0.92 → all RETHINKs high → ABSTAIN
 *   "improve:"  → σ starts 0.55, drops 0.10 each round → ACCEPT at r=2
 *   anything else → σ = 0.30 (RETHINK, then ACCEPT second try)
 */
typedef struct { int calls; } st_ctx_t;

static int st_generate(const char *prompt, int round, void *ctx,
                       const char **out_text, float *out_sigma,
                       double *out_cost_eur) {
    st_ctx_t *c = (st_ctx_t *)ctx;
    if (c != NULL) c->calls++;
    *out_cost_eur = 0.0001;  /* local electricity */
    if (strncmp(prompt, "low:", 4) == 0) {
        *out_text  = "low-sigma answer";
        *out_sigma = 0.05f;
    } else if (strncmp(prompt, "high:", 5) == 0) {
        *out_text  = "uncertain draft";
        *out_sigma = 0.92f;   /* always above τ_rethink → ABSTAIN */
    } else if (strncmp(prompt, "mid:", 4) == 0) {
        *out_text  = "plateau draft";
        *out_sigma = 0.55f;   /* always RETHINK until budget exhausted */
    } else if (strncmp(prompt, "improve:", 8) == 0) {
        *out_text  = "improving answer";
        /* 0.55, 0.45, 0.35 — drops under τ_rethink=0.60 always,
         * crosses τ_accept=0.40 at round 2 with tau_accept=0.40. */
        float table[] = { 0.55f, 0.45f, 0.35f };
        int r = round < 3 ? round : 2;
        *out_sigma = table[r];
    } else {
        *out_text  = "default answer";
        *out_sigma = 0.30f;
    }
    return 0;
}

static int st_escalate(const char *prompt, void *ctx,
                       const char **out_text, float *out_sigma,
                       double *out_cost_eur,
                       uint64_t *out_bytes_sent,
                       uint64_t *out_bytes_recv) {
    (void)prompt; (void)ctx;
    *out_text         = "escalated cloud answer";
    *out_sigma        = 0.10f;
    *out_cost_eur     = 0.012;
    *out_bytes_sent   = 1024;
    *out_bytes_recv   = 4096;
    return 0;
}

/* -------- self-test -------- */

int cos_sigma_pipeline_self_test(void) {
    enum { N_SLOTS = 8 };
    cos_sigma_engram_entry_t slots[N_SLOTS];
    memset(slots, 0, sizeof(slots));
    cos_sigma_engram_t engram;
    if (cos_sigma_engram_init(&engram, slots, N_SLOTS, 0.25f, 100, 10) != 0) {
        fprintf(stderr, "pipeline: engram init failed\n");
        return 1;
    }

    cos_sigma_sovereign_t sovereign;
    cos_sigma_sovereign_init(&sovereign, 0.85f);

    cos_sigma_agent_t agent;
    cos_sigma_agent_init(&agent, 0.80f, 0.10f);

    st_ctx_t ctx = {0};
    cos_pipeline_config_t cfg;
    cos_sigma_pipeline_config_defaults(&cfg);
    cfg.tau_accept    = 0.40f;   /* wider ACCEPT band so "improve"
                                   * lands ACCEPT at round 2       */
    cfg.tau_rethink   = 0.60f;
    cfg.max_rethink   = 3;
    cfg.mode          = COS_PIPELINE_MODE_HYBRID;
    cfg.engram        = &engram;
    cfg.sovereign     = &sovereign;
    cfg.agent         = &agent;
    cfg.generate      = st_generate;
    cfg.generate_ctx  = &ctx;
    cfg.escalate      = st_escalate;

    int fails = 0;

    /* --- T1: ACCEPT on first try --- */
    cos_pipeline_result_t r;
    if (cos_sigma_pipeline_run(&cfg, "low:2+2", &r) != 0) fails++;
    if (r.final_action != COS_SIGMA_ACTION_ACCEPT) {
        fprintf(stderr, "T1: action=%d\n", r.final_action); fails++;
    }
    if (r.rethink_count != 0) { fprintf(stderr, "T1: rethink %d\n", r.rethink_count); fails++; }
    if (r.escalated) { fprintf(stderr, "T1: escalated\n"); fails++; }
    if (r.engram_hit) { fprintf(stderr, "T1: hit on miss\n"); fails++; }

    /* --- T2: engram HIT on the second run of the same prompt --- */
    if (cos_sigma_pipeline_run(&cfg, "low:2+2", &r) != 0) fails++;
    if (!r.engram_hit) { fprintf(stderr, "T2: miss on HIT\n"); fails++; }
    if (r.final_action != COS_SIGMA_ACTION_ACCEPT) fails++;
    if (r.cost_eur != 0.0) { fprintf(stderr, "T2: cost %.6f\n", r.cost_eur); fails++; }

    /* --- T3: RETHINK → ACCEPT after σ drops --- */
    if (cos_sigma_pipeline_run(&cfg, "improve:explain X", &r) != 0) fails++;
    if (r.final_action != COS_SIGMA_ACTION_ACCEPT) {
        fprintf(stderr, "T3: action=%d rethink=%d sigma=%.3f\n",
                r.final_action, r.rethink_count, r.sigma); fails++;
    }
    if (r.rethink_count < 1) {
        fprintf(stderr, "T3: rethink_count=%d\n", r.rethink_count); fails++;
    }
    if (r.escalated) { fprintf(stderr, "T3: escalated\n"); fails++; }

    /* --- T4: plateau σ → exhaust rethinks → ESCALATE ---
     * σ stays at 0.55 on every round; reinforce_round forces
     * ABSTAIN at round == MAX_ROUNDS - 1, triggering escalation. */
    if (cos_sigma_pipeline_run(&cfg, "mid:P=NP?", &r) != 0) fails++;
    if (!r.escalated) { fprintf(stderr, "T4: not escalated\n"); fails++; }
    if (r.rethink_count != COS_SIGMA_REINFORCE_MAX_ROUNDS - 1) {
        fprintf(stderr, "T4: rethink_count=%d (want %d)\n",
                r.rethink_count, COS_SIGMA_REINFORCE_MAX_ROUNDS - 1); fails++;
    }

    /* --- T5: LOCAL_ONLY mode forbids escalation --- */
    cfg.mode = COS_PIPELINE_MODE_LOCAL_ONLY;
    if (cos_sigma_pipeline_run(&cfg, "high:still no", &r) != 0) fails++;
    if (r.escalated) { fprintf(stderr, "T5: escalated in LOCAL_ONLY\n"); fails++; }
    if (r.final_action != COS_SIGMA_ACTION_ABSTAIN) {
        fprintf(stderr, "T5: action=%d\n", r.final_action); fails++;
    }
    /* DEV-2: even a "persistently high σ" prompt must exercise the
     * full regeneration budget before we ABSTAIN.  The old behavior
     * short-circuited to ABSTAIN at round 0; the new behavior tries
     * MAX-1 regenerations first.  The "high:" stub returns σ=0.92
     * on every call, so rethink_count must equal budget - 1. */
    if (r.rethink_count != COS_SIGMA_REINFORCE_MAX_ROUNDS - 1) {
        fprintf(stderr, "T5: rethink_count=%d (want %d after DEV-2)\n",
                r.rethink_count, COS_SIGMA_REINFORCE_MAX_ROUNDS - 1); fails++;
    }

    /* --- T6: sovereign counters moved forward --- */
    if (sovereign.n_local == 0) {
        fprintf(stderr, "T6: no local calls\n"); fails++;
    }
    /* T4 fired a cloud call, so n_cloud must be ≥ 1. */
    if (sovereign.n_cloud != 1) {
        fprintf(stderr, "T6: n_cloud=%u (want 1)\n", sovereign.n_cloud);
        fails++;
    }

    /* --- T7: agent gate: WRITE at σ=0.05 ALLOWs; σ=0.50 does not --- */
    cfg.mode = COS_PIPELINE_MODE_HYBRID;
    if (cos_sigma_pipeline_run(&cfg, "low:x", &r) != 0) fails++;
    if (r.agent_gate != COS_AGENT_ALLOW) {
        fprintf(stderr, "T7a: gate=%d\n", r.agent_gate); fails++;
    }

    cos_sigma_pipeline_free_engram_values(&engram);
    return fails;
}
