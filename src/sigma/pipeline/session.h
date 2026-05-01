/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Session — long-running agent session state.
 *
 * Cursor, Claude Code, Codex, every coding assistant in 2026 hits
 * the same wall: context fills up, the model forgets the task,
 * the user starts a new session.  σ-Session keeps the session
 * alive by:
 *
 *   1. Recording every turn  — (prompt, response, σ, cost, t).
 *   2. σ-priority trimming   — when the context window is full,
 *                              drop the HIGHEST-σ turns first
 *                              (least confident answers were
 *                              least useful).
 *   3. Trend tracking        — rolling mean σ, slope, cost.  If
 *                              σ is trending up, the model is
 *                              drifting; the runtime can offer
 *                              an engram flush or an escalation.
 *   4. Checkpoint save/load  — the full state serializes to a
 *                              deterministic line-based format
 *                              (no JSON dependency) so `cos chat
 *                              --save session.txt` and `--load
 *                              session.txt` give perfect resume.
 *
 * σ is the only ranking signal — no recency decay, no LRU.  An
 * old low-σ turn beats a fresh high-σ one, because the low-σ turn
 * is more trustworthy context for the next answer.
 *
 * Storage model:
 *   Fixed-capacity ring of turns (caller-owned).  When count > cap,
 *   the WORST-σ turn is evicted, not the oldest.
 *
 * Contracts (v0):
 *   1. init rejects NULL storage or cap < 2.
 *   2. append copies the prompt/response pointers by VALUE (the
 *      buffers must outlive the session, OR the caller can use
 *      the dup variant).
 *   3. trend reports (mean, slope, max, min, last_N_delta).
 *   4. save/load are human-readable line form: one turn per block,
 *      exact round-trip for all fields.  Checksum header detects
 *      corruption.
 *   5. Self-test covers init guards, σ-priority eviction, trend
 *      math, and save→load round-trip.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SESSION_H
#define COS_SIGMA_SESSION_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SESSION_PROMPT_CAP    512
#define COS_SESSION_RESPONSE_CAP  1024

typedef struct {
    char     prompt[COS_SESSION_PROMPT_CAP];
    char     response[COS_SESSION_RESPONSE_CAP];
    float    sigma;
    double   cost_eur;
    uint64_t t_ns;             /* caller-supplied monotonic stamp  */
    int      turn_id;          /* monotonically increasing          */
    int      escalated;        /* 1 if the turn went cloud-side    */
} cos_session_turn_t;

typedef struct {
    cos_session_turn_t *slots;
    int                 cap;
    int                 count;
    int                 next_turn_id;
    int                 n_evicted;
    double              total_cost_eur;
    float               sigma_mean;
    float               sigma_slope;   /* (last half mean) − (first half mean) */
    float               sigma_min;
    float               sigma_max;
} cos_session_t;

int cos_sigma_session_init(cos_session_t *s,
                           cos_session_turn_t *storage, int cap);

int cos_sigma_session_append(cos_session_t *s,
                             const char *prompt,
                             const char *response,
                             float sigma, double cost_eur,
                             uint64_t t_ns, int escalated);

/* Returns the current number of live turns. */
int cos_sigma_session_size(const cos_session_t *s);

/* Recompute trend stats (mean, slope, min, max) from live turns.
 * No-op if count == 0. */
void cos_sigma_session_recompute_trend(cos_session_t *s);

/* "Is the model drifting?" — returns 1 iff sigma_slope > threshold
 * AND count >= min_count. */
int cos_sigma_session_is_drifting(const cos_session_t *s,
                                  float slope_thresh, int min_count);

/* Save the session to a file stream (line-based).  Format:
 *     COS-SESSION 1 <count> <next_id> <n_evicted> <total_cost> <checksum>
 *     T <turn_id> <escalated> <sigma> <cost> <t_ns>
 *     P <prompt-escaped>
 *     R <response-escaped>
 *     ...
 * Escaping: replace `\n` with `\\n` and `\\` with `\\\\`.   */
int cos_sigma_session_save(const cos_session_t *s, FILE *out);

/* Mirror of save. Caller supplies storage buffers. */
int cos_sigma_session_load(cos_session_t *s,
                           cos_session_turn_t *storage, int cap,
                           FILE *in);

int cos_sigma_session_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SESSION_H */
