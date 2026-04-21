/*
 * sigma_pipeline — end-to-end composition of the 20 σ-primitives.
 *
 *      Input
 *        ↓
 *      [0] Codex loaded as system prompt (I0)      — optional
 *      [1] Sovereign mode check (LOCAL / HYBRID)   — P20
 *      [2] Multimodal detection                    — P12
 *      [3] Engram lookup  → HIT → return (~0.5 ns) — P7
 *        ↓ MISS
 *      [4] Generate (BitNet-sized local call)      — generator cb
 *      [5] σ-measure
 *      [6] MoE width adjust + regenerate           — P11
 *      [7] σ-gate: ACCEPT / RETHINK / ABSTAIN      — P1
 *        ↓ RETHINK (≤ N rounds)
 *      [8] TTT (fast weights) + Continual (freeze) — P6 / P16
 *        ↓ regenerate → re-measure
 *      [9] Escalate (swarm or API)                 — P2 / P14
 *     [10] Agent autonomy gate                     — P18
 *     [11] Diagnostic (entropy / gap / max-prob)   — P19
 *     [12] Live τ adaptation                       — P15
 *     [13] Engram store                            — P7
 *     [14] Sovereign cost update                   — P20
 *        ↓
 *      Response + σ + cost + diagnostic
 *
 * This primitive does NOT ship a BitNet runtime.  It ships the
 * CONTROL plane: the logic that decides which primitive fires
 * when, and in what order.  Text generation is injected via a
 * `cos_pipeline_generator_fn` callback so:
 *
 *   * Integration tests can replace the generator with a
 *     deterministic stub that returns pre-programmed (text, σ)
 *     tuples covering every branch (engram hit, RETHINK×3,
 *     ABSTAIN, escalation, etc).
 *   * `cos chat` / `cos benchmark` will later plug in a real
 *     BitNet bridge using the same signature.
 *
 * The pipeline owns zero model weights and zero persistent state
 * of its own — engram slots, sovereign counters, live-tau state
 * etc. are all caller-provided buffers.  That keeps the
 * primitive allocation-free on the hot path and lets the CLI
 * layer decide lifetime / persistence.
 *
 * Contracts (v0):
 *
 *  1. Every run returns a well-formed result with well-defined
 *     values for response / sigma / action / rethink_count /
 *     escalated / cost_eur.
 *  2. Engram HIT short-circuits generation; rethink_count == 0,
 *     escalated == false, cost_eur == 0 on that path.
 *  3. RETHINK loop has a hard budget of
 *     COS_SIGMA_REINFORCE_MAX_ROUNDS; exceeding it transitions
 *     to ABSTAIN → (if mode allows) escalate.
 *  4. MODE_LOCAL_ONLY never escalates, even on ABSTAIN.  In that
 *     case response is a stock "uncertain" string.
 *  5. Agent check never ALLOWs on σ above the class threshold.
 *  6. Cost fields are non-negative and monotone (counters never
 *     go backwards).
 *  7. Self-test exercises every branch and returns 0 on pass.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PIPELINE_H
#define COS_SIGMA_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "agent.h"
#include "codex.h"
#include "engram.h"
#include "reinforce.h"
#include "sovereign.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- configuration --- */

typedef enum {
    COS_PIPELINE_MODE_LOCAL_ONLY = 0, /* never escalate             */
    COS_PIPELINE_MODE_HYBRID     = 1, /* escalate on ABSTAIN        */
    COS_PIPELINE_MODE_SWARM      = 2, /* escalate via multi-model   */
} cos_pipeline_mode_t;

/* Single generator call.  The callback is given:
 *
 *   prompt       the user input (may be augmented by the RETHINK
 *                suffix on the second attempt)
 *   round        0 on first attempt, increments on each RETHINK
 *   ctx          caller-provided opaque pointer
 *   out_text     must be set to a NUL-terminated string (caller
 *                retains ownership; pipeline will NOT free it)
 *   out_sigma    must be set to the measured σ for this attempt
 *   out_cost_eur must be set to the local-call cost estimate
 *                (electricity only for BitNet, ~€0.0001)
 *
 * Return 0 on success, non-zero on hard failure (treated as
 * ABSTAIN).  Non-zero cost_eur from the callback is forwarded to
 * the sovereign ledger. */
typedef int (*cos_pipeline_generator_fn)(
    const char *prompt, int round, void *ctx,
    const char **out_text, float *out_sigma,
    double *out_cost_eur);

/* Escalation callback: invoked when the local pipeline has given
 * up and mode allows cloud / swarm calls.  Same output shape as
 * the generator.  Cost is forwarded to the sovereign ledger as a
 * CLOUD call (vs local for the generator). */
typedef int (*cos_pipeline_escalator_fn)(
    const char *prompt, void *ctx,
    const char **out_text, float *out_sigma,
    double *out_cost_eur,
    uint64_t *out_bytes_sent, uint64_t *out_bytes_recv);

typedef struct {
    /* σ thresholds. */
    float tau_accept;     /* default 0.20 */
    float tau_rethink;    /* default 0.60 */

    /* Rethink budget (<= COS_SIGMA_REINFORCE_MAX_ROUNDS). */
    int   max_rethink;

    /* Runtime mode. */
    cos_pipeline_mode_t mode;

    /* Optional Codex (soul).  When set, its hash is echoed back
     * in the result for audit — proves the same soul was loaded
     * across runs.  Callers pre-load with cos_sigma_codex_load. */
    const cos_sigma_codex_t *codex;

    /* Shared caller-owned engram (cache).  May be NULL to skip
     * both lookup and store. */
    cos_sigma_engram_t *engram;

    /* Shared sovereign ledger.  May be NULL to skip cost track. */
    cos_sigma_sovereign_t *sovereign;

    /* Shared agent state.  May be NULL to skip autonomy gate. */
    cos_sigma_agent_t *agent;

    /* Callbacks. */
    cos_pipeline_generator_fn generate;
    void                     *generate_ctx;
    cos_pipeline_escalator_fn escalate;
    void                     *escalate_ctx;

    /* DEV-3: persistence hook.  Called after pipeline_engram_store
     * successfully stores a NEW (prompt, response, sigma) entry in
     * the in-memory engram.  Typical implementation: write-through
     * to ~/.cos/engram.db via SQLite.  Optional — when NULL, engram
     * behaves as before (in-memory only, evaporates on process
     * exit).  Return value is ignored; errors should be logged by
     * the hook.  Invocation order: engram.put succeeds → hook runs
     * (so the in-memory cache is always the source of truth; the
     * persistence layer is a replica). */
    void (*on_engram_store)(const char *prompt,
                            uint64_t    prompt_hash,
                            const char *response,
                            float       sigma,
                            void       *ctx);
    void *on_engram_store_ctx;
} cos_pipeline_config_t;

/* --- result --- */

typedef struct {
    const char        *response;     /* caller-owned text       */
    float              sigma;        /* last measured σ         */
    bool               engram_hit;   /* lookup short-circuit    */
    int                rethink_count;/* 0 on ACCEPT / HIT       */
    bool               escalated;    /* triggered cloud path    */
    double             cost_eur;     /* this-turn cost          */
    cos_sigma_action_t final_action; /* ACCEPT / RETHINK / ABST */
    cos_agent_decision_t agent_gate; /* if agent != NULL        */
    const char        *diagnostic;   /* short one-liner         */
    cos_pipeline_mode_t mode;
    uint64_t           codex_hash;   /* 0 if no codex loaded    */
} cos_pipeline_result_t;

/* --- entry point --- */

/* Initialise a config with canonical defaults.  Callers are
 * expected to plug in generate / escalate (+ optional engram,
 * sovereign, agent, codex) before the first run. */
void cos_sigma_pipeline_config_defaults(cos_pipeline_config_t *cfg);

/* Run one turn.  Fills `*out` with the result.  Returns 0 on a
 * logically-successful turn (whatever the action was — ACCEPT,
 * ABSTAIN, escalated all count); returns non-zero only on
 * pipeline-level failures (NULL config, missing generator, etc). */
int cos_sigma_pipeline_run(cos_pipeline_config_t *cfg,
                           const char *input,
                           cos_pipeline_result_t *out);

/* Free every heap-allocated value currently stored in the engram
 * cache.  The pipeline strdups response text on store; this helper
 * drains those allocations at shutdown before the engram slots
 * themselves go out of scope.  Safe on empty / zero-initialised
 * engrams. */
void cos_sigma_pipeline_free_engram_values(cos_sigma_engram_t *e);

/* Canonical self-test: exercises engram HIT, ACCEPT, RETHINK → ACCEPT,
 * RETHINK → ABSTAIN → escalate, LOCAL_ONLY ABSTAIN, agent block,
 * cost tracking.  Returns 0 on pass. */
int cos_sigma_pipeline_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_PIPELINE_H */
