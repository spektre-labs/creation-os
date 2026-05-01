/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-RateLimit — see ratelimit.h for mechanism and contract notes.
 *
 * Deterministic: no RNG, no allocation.  The caller owns the peer
 * storage; every time windowing operation derives from the
 * `now_ns` argument the caller passes in, so the self-test
 * reproduces identical state on every host.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ratelimit.h"

#include <string.h>

/* ------------------------------------------------------------------ *
 *  Helpers
 * ------------------------------------------------------------------ */

#define NS_PER_MINUTE (60ULL * 1000000000ULL)
#define NS_PER_HOUR   (3600ULL * 1000000000ULL)

static inline int is_finite_f(float v) { return v == v; }

static inline float clip01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static cos_rl_peer_t *find_slot(cos_rl_t *rl, const char *id) {
    for (int i = 0; i < rl->count; i++)
        if (strncmp(rl->slots[i].node_id, id, COS_RL_ID_CAP) == 0)
            return &rl->slots[i];
    return NULL;
}

/* Evict the least-reputation, not-recently-seen, non-trusted peer.
 * Returns the slot, or NULL if no eviction candidate exists. */
static cos_rl_peer_t *try_evict(cos_rl_t *rl, uint64_t now_ns) {
    cos_rl_peer_t *victim = NULL;
    float worst_r = 2.0f;
    for (int i = 0; i < rl->count; i++) {
        cos_rl_peer_t *p = &rl->slots[i];
        if (p->reputation >= rl->r_good) continue;           /* trusted    */
        if (now_ns - p->last_seen_ns < rl->eviction_grace_ns) continue;
        if (p->reputation < worst_r) { worst_r = p->reputation; victim = p; }
    }
    return victim;
}

static cos_rl_peer_t *alloc_slot(cos_rl_t *rl, const char *id, uint64_t now_ns) {
    cos_rl_peer_t *slot = NULL;
    if (rl->count < rl->cap) {
        slot = &rl->slots[rl->count++];
    } else {
        slot = try_evict(rl, now_ns);
        if (!slot) return NULL;
    }
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->node_id, id, COS_RL_ID_CAP - 1);
    slot->node_id[COS_RL_ID_CAP - 1] = '\0';
    slot->first_seen_ns          = now_ns;
    slot->last_seen_ns           = now_ns;
    slot->reputation             = 0.50f;      /* neutral start */
    slot->minute_window_start_ns = now_ns;
    slot->hour_window_start_ns   = now_ns;
    return slot;
}

/* ------------------------------------------------------------------ *
 *  Public API
 * ------------------------------------------------------------------ */

int cos_sigma_rl_init(cos_rl_t *rl,
                      cos_rl_peer_t *storage, int cap,
                      int max_per_minute,
                      int max_per_hour,
                      float r_ban,
                      float r_good,
                      float give_min,
                      float sigma_reject_peer,
                      uint64_t eviction_grace_ns)
{
    if (!rl || !storage || cap <= 0)                       return -1;
    if (max_per_minute <= 0 || max_per_hour <= 0)          return -1;
    if (max_per_hour < max_per_minute)                     return -1;
    if (!(r_ban >= 0.0f) || !(r_good <= 1.0f))             return -1;
    if (!(r_ban < r_good))                                 return -1;
    if (!(give_min >= 0.0f) || !(give_min <= 1.0f))        return -1;
    if (!is_finite_f(sigma_reject_peer))                   return -1;
    if (!(sigma_reject_peer >= 0.0f) ||
        !(sigma_reject_peer <= 1.0f))                      return -1;

    memset(rl, 0, sizeof(*rl));
    rl->slots              = storage;
    rl->cap                = cap;
    rl->max_per_minute     = max_per_minute;
    rl->max_per_hour       = max_per_hour;
    rl->r_ban              = r_ban;
    rl->r_good             = r_good;
    rl->give_min           = give_min;
    rl->elevated_mult      = 2;
    rl->sigma_reject_peer  = sigma_reject_peer;
    rl->reputation_gain    = 0.05f;
    rl->reputation_loss    = 0.10f;
    rl->spam_loss          = 0.15f;
    rl->eviction_grace_ns  = eviction_grace_ns ?
                              eviction_grace_ns : (10ULL * NS_PER_MINUTE);
    return 0;
}

cos_rl_peer_t *cos_sigma_rl_find(cos_rl_t *rl, const char *id) {
    if (!rl || !id) return NULL;
    return find_slot(rl, id);
}

const char *cos_sigma_rl_decision_name(int d) {
    switch (d) {
    case COS_RL_ALLOWED:         return "ALLOWED";
    case COS_RL_THROTTLED_RATE:  return "THROTTLED_RATE";
    case COS_RL_THROTTLED_GIVE:  return "THROTTLED_GIVE";
    case COS_RL_BLOCKED:         return "BLOCKED";
    default:                     return "UNKNOWN";
    }
}

static int quota_for(const cos_rl_t *rl, const cos_rl_peer_t *p) {
    return (p->reputation >= rl->r_good) ?
           (rl->max_per_minute * rl->elevated_mult) :
           rl->max_per_minute;
}
static int hour_quota_for(const cos_rl_t *rl, const cos_rl_peer_t *p) {
    return (p->reputation >= rl->r_good) ?
           (rl->max_per_hour * rl->elevated_mult) :
           rl->max_per_hour;
}

/* Roll the minute / hour sliding windows based on `now_ns`. */
static void roll_windows(cos_rl_peer_t *p, uint64_t now_ns) {
    if (now_ns - p->minute_window_start_ns >= NS_PER_MINUTE) {
        p->minute_window_start_ns = now_ns;
        p->queries_this_minute    = 0;
    }
    if (now_ns - p->hour_window_start_ns >= NS_PER_HOUR) {
        p->hour_window_start_ns = now_ns;
        p->queries_this_hour    = 0;
    }
}

int cos_sigma_rl_check(cos_rl_t *rl,
                       const char *node_id,
                       uint64_t now_ns,
                       cos_rl_report_t *report)
{
    if (report) memset(report, 0, sizeof(*report));
    if (!rl || !node_id || !node_id[0]) return -1;

    cos_rl_peer_t *p = find_slot(rl, node_id);
    int new_peer = 0;
    if (!p) {
        p = alloc_slot(rl, node_id, now_ns);
        if (!p) return -2;   /* capacity exhausted, no eviction candidate */
        new_peer = 1;
    }
    p->last_seen_ns = now_ns;
    roll_windows(p, now_ns);

    /* Decision order: BLOCKED ≻ THROTTLED_RATE ≻ THROTTLED_GIVE ≻ ALLOWED. */
    int decision = COS_RL_ALLOWED;
    if (p->banned || p->reputation < rl->r_ban) {
        p->banned = 1;
        p->block_strikes++;
        decision = COS_RL_BLOCKED;
    } else {
        int qpm = quota_for(rl, p);
        int qph = hour_quota_for(rl, p);
        if (p->queries_this_minute >= qpm || p->queries_this_hour >= qph) {
            p->throttle_strikes++;
            p->reputation = clip01(p->reputation - rl->spam_loss);
            decision = COS_RL_THROTTLED_RATE;
        } else if (p->queries_sent >= 10) {
            /* Free-rider gate only applies once the peer has asked
             * enough times that the ratio is meaningful. */
            float served = (float)p->answers_served;
            float sent   = (float)p->queries_sent;
            float gr     = (sent > 0.0f) ? (served / sent) : 1.0f;
            if (gr < rl->give_min) {
                p->throttle_strikes++;
                decision = COS_RL_THROTTLED_GIVE;
            }
        }
    }

    if (decision == COS_RL_ALLOWED) {
        p->queries_sent++;
        p->queries_this_minute++;
        p->queries_this_hour++;
    }

    if (report) {
        report->decision             = decision;
        report->peer                 = p;
        report->queries_this_minute  = p->queries_this_minute;
        report->queries_this_hour    = p->queries_this_hour;
        report->reputation           = p->reputation;
        report->give_ratio           = (p->queries_sent > 0) ?
            ((float)p->answers_served / (float)p->queries_sent) : 1.0f;
        report->quota_per_minute     = quota_for(rl, p);
        report->new_peer             = new_peer;
    }
    return 0;
}

int cos_sigma_rl_observe_answer(cos_rl_t *rl,
                                const char *node_id,
                                float sigma_answer,
                                int served_locally,
                                uint64_t now_ns)
{
    if (!rl || !node_id) return -1;
    if (!is_finite_f(sigma_answer))
        sigma_answer = 1.0f;                    /* treat NaN as worst     */
    sigma_answer = clip01(sigma_answer);

    cos_rl_peer_t *p = find_slot(rl, node_id);
    if (!p) {
        p = alloc_slot(rl, node_id, now_ns);
        if (!p) return -2;
    }
    p->last_seen_ns = now_ns;

    if (served_locally) {
        p->answers_served++;
    } else {
        /* The peer served us; reputation moves with σ. */
        if (sigma_answer >= rl->sigma_reject_peer) {
            p->reputation = clip01(p->reputation - rl->reputation_loss);
        } else {
            p->reputation = clip01(p->reputation + rl->reputation_gain *
                                     (1.0f - sigma_answer));
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Self-test — 5 canonical scenarios
 * ------------------------------------------------------------------ */

int cos_sigma_rl_self_test(void) {
    cos_rl_t rl;
    cos_rl_peer_t slots[8];
    if (cos_sigma_rl_init(&rl, slots, 8,
                          /*qpm=*/5, /*qph=*/60,
                          /*r_ban=*/0.10f, /*r_good=*/0.80f,
                          /*give_min=*/0.10f,
                          /*sigma_reject=*/0.70f,
                          /*evict_grace=*/0) != 0) return -1;

    uint64_t t = 1000ULL * 1000ULL * 1000ULL;   /* start at 1s            */
    cos_rl_report_t rep;

    /* 1. New peer accepted, reputation neutral. */
    if (cos_sigma_rl_check(&rl, "alice", t, &rep) != 0) return -2;
    if (rep.decision != COS_RL_ALLOWED) return -3;
    if (!rep.new_peer) return -4;

    /* 2. Spam: send 6 queries within a minute → 6th gets throttled. */
    for (int i = 0; i < 5; i++) {
        if (cos_sigma_rl_check(&rl, "spammer", t + (uint64_t)i, &rep) != 0)
            return -5;
        if (rep.decision != COS_RL_ALLOWED) return -6;
    }
    if (cos_sigma_rl_check(&rl, "spammer", t + 5, &rep) != 0) return -7;
    if (rep.decision != COS_RL_THROTTLED_RATE) return -8;

    /* 3. After the minute window rolls the same peer is allowed again. */
    if (cos_sigma_rl_check(&rl, "spammer", t + NS_PER_MINUTE + 1, &rep) != 0)
        return -9;
    if (rep.decision != COS_RL_ALLOWED) return -10;

    /* 4. σ-driven reputation drop → BLOCKED.  Feed four bad answers
     *    (σ = 0.95) so reputation 0.50 → 0.10 → banned. */
    for (int i = 0; i < 5; i++)
        cos_sigma_rl_observe_answer(&rl, "griefer", 0.95f, 0, t);
    if (cos_sigma_rl_check(&rl, "griefer", t, &rep) != 0) return -11;
    /* Depending on loss curve this may need one extra hit. */
    int j = 0;
    while (rep.decision != COS_RL_BLOCKED && j < 20) {
        cos_sigma_rl_observe_answer(&rl, "griefer", 0.95f, 0, t);
        cos_sigma_rl_check(&rl, "griefer", t, &rep);
        j++;
    }
    if (rep.decision != COS_RL_BLOCKED) return -12;

    /* 5. Free-rider: 10 allowed queries (one per minute so the
     *    rate gate never fires), then the 11th triggers
     *    THROTTLED_GIVE because queries_sent=10, answers_served=0. */
    for (int i = 0; i < 11; i++)
        cos_sigma_rl_check(&rl, "leech",
                           t + (uint64_t)(2 + i) * NS_PER_MINUTE, &rep);
    if (rep.decision != COS_RL_THROTTLED_GIVE) return -13;

    /* 6. Good peer (low-σ answer) gains reputation above r_good and
     *    earns elevated quota (2×). */
    cos_rl_peer_t *ap = NULL;
    for (int i = 0; i < 20; i++) {
        cos_sigma_rl_observe_answer(&rl, "trusted", 0.05f, 0, t);
        ap = cos_sigma_rl_find(&rl, "trusted");
        if (ap && ap->reputation >= rl.r_good) break;
    }
    if (!ap || ap->reputation < rl.r_good) return -14;
    /* Check reports elevated quota. */
    cos_sigma_rl_check(&rl, "trusted", t, &rep);
    if (rep.quota_per_minute != rl.max_per_minute * rl.elevated_mult) return -15;

    /* 7. Arg-errors. */
    if (cos_sigma_rl_init(NULL, slots, 8, 5, 60, 0.1f, 0.8f, 0.1f, 0.7f, 0) != -1) return -16;
    if (cos_sigma_rl_init(&rl, slots, 8, 5, 60, 0.9f, 0.1f, 0.1f, 0.7f, 0) != -1) return -17;
    if (cos_sigma_rl_check(&rl, NULL, t, &rep) != -1) return -18;

    return 0;
}
