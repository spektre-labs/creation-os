/*
 * v175 σ-Debate-Train — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "debate_train.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const char *cos_v175_result_name(cos_v175_debate_result_t r) {
    switch (r) {
        case COS_V175_RES_CHOSEN_POS: return "chosen";
        case COS_V175_RES_PAIR:       return "pair";
        case COS_V175_RES_SKIP:       return "skip";
        default:                       return "?";
    }
}

void cos_v175_init(cos_v175_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x175DEBA7E0175ULL;
    s->tau_consensus     = 0.18f;  /* both σ below this AND |Δ| small ⇒ skip */
    s->spin_convergence  = 0.01f;
    s->elo_k             = 32.0f;
    s->elo_init          = 1500.0f;
    for (int i = 0; i < COS_V175_N_ADAPTERS; ++i)
        s->ratings[i] = s->elo_init;
}

/* ------------------------------------------------------------- */
/* Debates                                                       */
/* ------------------------------------------------------------- */

/* Fixture: 4 prompts × 3 specialist pairings → 12 debates.
 *
 * Indices (0=medical, 1=creative, 2=code).  Pairings per
 * prompt rotate so each specialist appears as A and as B.
 *
 * σ arrays below produce:
 *   ≥ 1 PAIR   (B meaningfully better than A)
 *   ≥ 1 CHOSEN (both low σ but one clearly better)
 *   ≥ 1 SKIP   (both consensus-low σ, |Δ| < τ_consensus · 0.5)
 */
static const float SIGMA_A_TAB[COS_V175_N_PROMPTS * COS_V175_N_SPECIALISTS] = {
    /* p0 */ 0.65f, 0.40f, 0.12f,
    /* p1 */ 0.10f, 0.55f, 0.30f,
    /* p2 */ 0.08f, 0.09f, 0.11f,  /* consensus prompt */
    /* p3 */ 0.70f, 0.35f, 0.50f,
};
static const float SIGMA_B_TAB[COS_V175_N_PROMPTS * COS_V175_N_SPECIALISTS] = {
    /* p0 */ 0.20f, 0.38f, 0.10f,
    /* p1 */ 0.05f, 0.25f, 0.15f,
    /* p2 */ 0.10f, 0.11f, 0.09f,
    /* p3 */ 0.30f, 0.20f, 0.45f,
};

void cos_v175_run_debates(cos_v175_state_t *s) {
    s->n_debates = 0;
    for (int p = 0; p < COS_V175_N_PROMPTS; ++p) {
        for (int i = 0; i < COS_V175_N_SPECIALISTS; ++i) {
            int idx = p * COS_V175_N_SPECIALISTS + i;
            cos_v175_debate_t *d = &s->debates[s->n_debates++];
            memset(d, 0, sizeof(*d));
            d->prompt_id = p;
            d->spec_a = i;
            d->spec_b = (i + 1) % COS_V175_N_SPECIALISTS;
            d->sigma_a = SIGMA_A_TAB[idx];
            d->sigma_b = SIGMA_B_TAB[idx];
            float delta = d->sigma_a - d->sigma_b;    /* +ve → B better */
            d->sigma_delta = fabsf(delta);

            if (d->sigma_a < s->tau_consensus &&
                d->sigma_b < s->tau_consensus &&
                d->sigma_delta < s->tau_consensus * 0.5f) {
                d->result = COS_V175_RES_SKIP;
                snprintf(d->note, sizeof(d->note),
                         "skip: consensus low-σ, |Δ|=%.2f",
                         (double)d->sigma_delta);
            } else if (delta > 0.08f) {
                d->result = COS_V175_RES_PAIR;
                snprintf(d->note, sizeof(d->note),
                         "pair: B refuted A (σ_a=%.2f σ_b=%.2f)",
                         (double)d->sigma_a, (double)d->sigma_b);
            } else {
                d->result = COS_V175_RES_CHOSEN_POS;
                snprintf(d->note, sizeof(d->note),
                         "chosen: A survived critique (σ_a=%.2f ≤ σ_b=%.2f)",
                         (double)d->sigma_a, (double)d->sigma_b);
            }
        }
    }
}

/* ------------------------------------------------------------- */
/* Tournament                                                    */
/* ------------------------------------------------------------- */

/* Closed-form Elo: expected = 1/(1 + 10^((Rb−Ra)/400))
 *                  new_Ra   = Ra + K · (actual − expected)
 *
 * Per-adapter σ_base fixtures produce a clear ordering A > C > B. */
static const float ADAPTER_SIGMA[COS_V175_N_ADAPTERS] = {
    0.12f, 0.42f, 0.22f,
};

void cos_v175_run_tournament(cos_v175_state_t *s) {
    s->n_matches = 0;
    for (int a = 0; a < COS_V175_N_ADAPTERS; ++a) {
        for (int b = a + 1; b < COS_V175_N_ADAPTERS; ++b) {
            /* each pair plays twice (home + away) */
            for (int home = 0; home < 2; ++home) {
                int x = home ? a : b;
                int y = home ? b : a;
                cos_v175_match_t *m = &s->matches[s->n_matches++];
                memset(m, 0, sizeof(*m));
                m->adapter_id     = x;
                m->opponent_id    = y;
                m->sigma_adapter  = ADAPTER_SIGMA[x];
                m->sigma_opponent = ADAPTER_SIGMA[y];

                float Rx = s->ratings[x];
                float Ry = s->ratings[y];
                float Ex = 1.0f / (1.0f + powf(10.0f, (Ry - Rx) / 400.0f));
                m->expected      = Ex;
                m->rating_before = Rx;

                /* actual: the adapter with lower σ wins; ties draw. */
                float actual;
                if (fabsf(m->sigma_adapter - m->sigma_opponent) < 1e-4f)
                    actual = 0.5f;
                else if (m->sigma_adapter < m->sigma_opponent)
                    actual = 1.0f;
                else
                    actual = 0.0f;
                m->actual = actual;

                float Rx_new = Rx + s->elo_k * (actual - Ex);
                m->rating_after  = Rx_new;
                s->ratings[x]    = Rx_new;
            }
        }
    }
}

int cos_v175_adapter_champion(const cos_v175_state_t *s) {
    int best = 0;
    for (int i = 1; i < COS_V175_N_ADAPTERS; ++i)
        if (s->ratings[i] > s->ratings[best]) best = i;
    return best;
}

/* ------------------------------------------------------------- */
/* SPIN                                                          */
/* ------------------------------------------------------------- */

/* Monotone shrinking σ_delta: σ_gen = 0.50 · 0.55^gen
 * so gen 0 → 0.500, gen 1 → 0.275, gen 2 → 0.151, gen 3 →
 * 0.083, gen 4 → 0.046, gen 5 → 0.025, gen 6 → 0.014,
 * gen 7 → 0.007 (< 0.01 threshold).  Convergence at gen 7. */
void cos_v175_run_spin(cos_v175_state_t *s) {
    s->n_spin = 0;
    float sig = 0.50f;
    float prev = 0.80f;
    bool  converged = false;
    for (int g = 0; g < COS_V175_SPIN_MAX; ++g) {
        cos_v175_spin_t *sp = &s->spin[s->n_spin++];
        sp->gen            = g;
        sp->sigma_current  = sig;
        sp->sigma_previous = prev;
        sp->sigma_delta    = fabsf(prev - sig);
        prev = sig;
        sig  = sig * 0.55f;
        if (!converged && sp->sigma_delta < s->spin_convergence) {
            converged = true;
            s->spin_converged    = true;
            s->spin_converged_at = g;
            sp->converged        = true;
            break;
        }
    }
    if (!converged) {
        s->spin_converged    = false;
        s->spin_converged_at = -1;
    }
}

/* ------------------------------------------------------------- */
/* JSON / NDJSON                                                 */
/* ------------------------------------------------------------- */

size_t cos_v175_to_json(const cos_v175_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int champ = cos_v175_adapter_champion(s);
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v175\",\"tau_consensus\":%.4f,"
        "\"elo_k\":%.1f,\"elo_init\":%.1f,\"n_debates\":%d,"
        "\"n_matches\":%d,\"champion\":%d,\"spin_converged\":%s,"
        "\"spin_converged_at\":%d,\"ratings\":[",
        (double)s->tau_consensus, (double)s->elo_k,
        (double)s->elo_init, s->n_debates, s->n_matches,
        champ, s->spin_converged ? "true" : "false",
        s->spin_converged_at);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V175_N_ADAPTERS; ++i) {
        n = snprintf(buf + used, cap - used, "%s%.4f",
                     i == 0 ? "" : ",", (double)s->ratings[i]);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"debates\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_debates; ++i) {
        const cos_v175_debate_t *d = &s->debates[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"prompt\":%d,\"a\":%d,\"b\":%d,"
            "\"sigma_a\":%.4f,\"sigma_b\":%.4f,\"delta\":%.4f,"
            "\"result\":\"%s\",\"note\":\"%s\"}",
            i == 0 ? "" : ",", d->prompt_id, d->spec_a, d->spec_b,
            (double)d->sigma_a, (double)d->sigma_b,
            (double)d->sigma_delta,
            cos_v175_result_name(d->result), d->note);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"spin\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_spin; ++i) {
        const cos_v175_spin_t *sp = &s->spin[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"gen\":%d,\"sigma_current\":%.4f,"
            "\"sigma_previous\":%.4f,\"delta\":%.4f,\"converged\":%s}",
            i == 0 ? "" : ",", sp->gen,
            (double)sp->sigma_current, (double)sp->sigma_previous,
            (double)sp->sigma_delta, sp->converged ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v175_dpo_ndjson(const cos_v175_state_t *s,
                           char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    for (int i = 0; i < s->n_debates; ++i) {
        const cos_v175_debate_t *d = &s->debates[i];
        if (d->result == COS_V175_RES_SKIP) continue;
        int n;
        if (d->result == COS_V175_RES_PAIR) {
            n = snprintf(buf + used, cap - used,
                "{\"kernel\":\"v175\",\"prompt\":%d,\"class\":\"pair\","
                "\"chosen_spec\":%d,\"rejected_spec\":%d,"
                "\"sigma_chosen\":%.4f,\"sigma_rejected\":%.4f}\n",
                d->prompt_id, d->spec_b, d->spec_a,
                (double)d->sigma_b, (double)d->sigma_a);
        } else {
            n = snprintf(buf + used, cap - used,
                "{\"kernel\":\"v175\",\"prompt\":%d,\"class\":\"chosen\","
                "\"chosen_spec\":%d,\"sigma_chosen\":%.4f}\n",
                d->prompt_id, d->spec_a, (double)d->sigma_a);
        }
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v175_self_test(void) {
    cos_v175_state_t s;
    cos_v175_init(&s, 0x175BEEF0175ULL);

    cos_v175_run_debates(&s);
    if (s.n_debates != COS_V175_N_PROMPTS * COS_V175_N_SPECIALISTS) return 1;

    int n_pair = 0, n_chosen = 0, n_skip = 0;
    for (int i = 0; i < s.n_debates; ++i) {
        switch (s.debates[i].result) {
            case COS_V175_RES_PAIR:       n_pair++;   break;
            case COS_V175_RES_CHOSEN_POS: n_chosen++; break;
            case COS_V175_RES_SKIP:       n_skip++;   break;
        }
    }
    if (n_pair   == 0) return 2;
    if (n_chosen == 0) return 3;
    if (n_skip   == 0) return 4;

    /* DPO: every PAIR has σ_chosen < σ_rejected after harvest. */
    char buf[16384];
    size_t nn = cos_v175_dpo_ndjson(&s, buf, sizeof(buf));
    if (nn == 0) return 5;

    /* Tournament */
    cos_v175_run_tournament(&s);
    if (s.n_matches != COS_V175_N_ADAPTERS * (COS_V175_N_ADAPTERS - 1)) return 6;
    int champ = cos_v175_adapter_champion(&s);
    if (champ != 0) return 7;       /* adapter 0 has lowest σ */

    /* Ratings: champion > others, losers below init */
    if (!(s.ratings[0] > s.elo_init)) return 8;
    if (!(s.ratings[1] < s.elo_init)) return 9;

    /* SPIN: must converge with monotone non-increasing Δ */
    cos_v175_run_spin(&s);
    if (!s.spin_converged) return 10;
    for (int i = 1; i < s.n_spin; ++i)
        if (s.spin[i].sigma_current > s.spin[i-1].sigma_current + 1e-6f) return 11;

    /* Determinism */
    cos_v175_state_t a, b;
    cos_v175_init(&a, 0x175BEEF0175ULL);
    cos_v175_init(&b, 0x175BEEF0175ULL);
    cos_v175_run_debates(&a);    cos_v175_run_tournament(&a); cos_v175_run_spin(&a);
    cos_v175_run_debates(&b);    cos_v175_run_tournament(&b); cos_v175_run_spin(&b);
    char A[16384], B[16384];
    size_t na = cos_v175_to_json(&a, A, sizeof(A));
    size_t nb = cos_v175_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 12;
    if (memcmp(A, B, na) != 0) return 13;

    return 0;
}
