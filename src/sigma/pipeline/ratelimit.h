/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-RateLimit — reputation-weighted rate limiting + abuse guard
 * for the σ-Protocol mesh.
 *
 * The D-series mesh (v273 split / v274 marketplace / v275 mesh /
 * v276 federation / v277 protocol) is an open system — any node
 * with a keypair can join, query, and serve.  Without a
 * reputation + throttle layer the mesh is a DoS target (spam
 * floods), a free-rider target (ask-only / never-serve), and a
 * griefing target (signed-but-low-quality answers).  σ-RateLimit
 * is the runtime that prices each interaction in reputation:
 *
 *   - Every peer carries a reputation r ∈ [0, 1].
 *   - r > r_good                  ⇒ elevated quota.
 *   - r_ban ≤ r ≤ r_good          ⇒ baseline quota.
 *   - r < r_ban                   ⇒ BLOCKED (frame dropped before
 *                                  σ-Protocol decoder runs).
 *   - give_ratio = served / sent  ⇒ free-rider tilt:
 *       give_ratio < give_min     ⇒ THROTTLED even with good r.
 *   - Sliding minute / hour windows (monotonic clock, not wall
 *     clock) cap query rate per peer.
 *
 * The reputation update function is σ-driven: a peer whose
 * answer came back with low σ (confident, on-topic) gains;
 * a peer whose answer came back with σ ≥ σ_reject_peer loses.
 * No manual moderation; the σ-gate IS the moderator.
 *
 * API shape mirrors σ-federation: a caller-owned storage array
 * plus an opaque registry struct, deterministic behaviour, JSON-
 * friendly report out-parameter.  All inputs NaN-guarded; caller
 * passes in monotonic nanoseconds (clock-independent tests).
 *
 * Threading: single-threaded.  Caller serialises access.
 *
 * Contracts (v0):
 *   1. init enforces positive caps, r_ban < r_good ≤ 1.0,
 *      give_min ∈ [0, 1].
 *   2. observe_query / observe_answer update counters + r.
 *   3. check enforces decision order BLOCKED ≻ THROTTLED_RATE ≻
 *      THROTTLED_GIVE ≻ ALLOWED.  Ties resolved deterministically
 *      (order above == decision priority).
 *   4. On peer eviction (capacity pressure) the lowest-reputation
 *      entry that has not been touched within eviction_grace_ns is
 *      reclaimed.  Trusted peers (r ≥ r_good) are exempt.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_RATELIMIT_H
#define COS_SIGMA_RATELIMIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_RL_ID_CAP 48

enum cos_rl_decision {
    COS_RL_ALLOWED         = 0,
    COS_RL_THROTTLED_RATE  = 1,   /* exceeded per-minute or per-hour cap    */
    COS_RL_THROTTLED_GIVE  = 2,   /* give_ratio < give_min for this peer    */
    COS_RL_BLOCKED         = 3,   /* reputation below r_ban                 */
};

typedef struct {
    char     node_id[COS_RL_ID_CAP];
    uint64_t first_seen_ns;
    uint64_t last_seen_ns;
    int      queries_sent;           /* this peer → local                   */
    int      answers_served;         /* local → this peer                   */
    int      queries_this_minute;
    uint64_t minute_window_start_ns;
    int      queries_this_hour;
    uint64_t hour_window_start_ns;
    float    reputation;             /* [0, 1]                              */
    int      banned;                 /* latched once r fell below r_ban     */
    int      throttle_strikes;       /* diagnostic: #times throttled        */
    int      block_strikes;          /* diagnostic: #times blocked          */
} cos_rl_peer_t;

typedef struct {
    cos_rl_peer_t *slots;
    int            cap;
    int            count;

    /* Thresholds. */
    int      max_per_minute;
    int      max_per_hour;
    float    r_ban;                  /* below → BLOCKED                    */
    float    r_good;                 /* above → elevated quota             */
    float    give_min;               /* give_ratio floor                    */
    int      elevated_mult;          /* quota × this when r ≥ r_good       */

    /* σ-reputation tuning. */
    float    sigma_reject_peer;      /* answer σ ≥ this ⇒ reputation drop  */
    float    reputation_gain;        /* per good answer                     */
    float    reputation_loss;        /* per bad answer                      */
    float    spam_loss;              /* per THROTTLED_RATE strike           */

    /* Eviction. */
    uint64_t eviction_grace_ns;
} cos_rl_t;

typedef struct {
    int      decision;               /* cos_rl_decision                     */
    const cos_rl_peer_t *peer;       /* may be NULL on BLOCKED-by-unknown   */
    int      queries_this_minute;
    int      queries_this_hour;
    float    reputation;
    float    give_ratio;
    int      quota_per_minute;       /* the cap this peer is measured to   */
    int      new_peer;               /* 1 iff peer was just registered      */
} cos_rl_report_t;

/* -------- Lifecycle ------------------------------------------------ */

int cos_sigma_rl_init(cos_rl_t *rl,
                      cos_rl_peer_t *storage, int cap,
                      int max_per_minute,
                      int max_per_hour,
                      float r_ban,
                      float r_good,
                      float give_min,
                      float sigma_reject_peer,
                      uint64_t eviction_grace_ns);

/* -------- Event sinks --------------------------------------------- */

/* Report that `node_id` just sent us a query at monotonic
 * timestamp `now_ns`.  Returns the decision and populates
 * `report` if non-NULL.  Counters are updated only on ALLOWED;
 * BLOCKED / THROTTLED_* strikes are recorded separately. */
int cos_sigma_rl_check(cos_rl_t *rl,
                       const char *node_id,
                       uint64_t now_ns,
                       cos_rl_report_t *report);

/* Reflect a completed answer into reputation: σ_answer low ⇒ gain,
 * high ⇒ loss.  served_locally = 1 ⇔ we served them; = 0 ⇔ they
 * served us (give-ratio grows on their side). */
int cos_sigma_rl_observe_answer(cos_rl_t *rl,
                                const char *node_id,
                                float sigma_answer,
                                int served_locally,
                                uint64_t now_ns);

/* -------- Introspection ------------------------------------------- */

cos_rl_peer_t *cos_sigma_rl_find(cos_rl_t *rl, const char *node_id);
const char    *cos_sigma_rl_decision_name(int d);

int cos_sigma_rl_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_RATELIMIT_H */
