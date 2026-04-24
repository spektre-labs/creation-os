/*
 * σ-driven autonomy modes (IDLE / LEARNING / EXPLORING / PRACTICING / SERVING).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "autonomy.h"

#include "omega_loop.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int                       g_cfg_autonomy;
static struct cos_autonomy_state *g_bind;

static int64_t aut_now_ms(void) {
    return (int64_t)time(NULL) * 1000LL;
}

static const char *mode_label(int m) {
    switch (m) {
    case COS_AUTONOMY_IDLE: return "IDLE";
    case COS_AUTONOMY_LEARNING: return "LEARNING";
    case COS_AUTONOMY_PRACTICING: return "PRACTICING";
    case COS_AUTONOMY_EXPLORING: return "EXPLORING";
    case COS_AUTONOMY_SERVING: return "SERVING";
    default: return "?";
    }
}

void cos_autonomy_configure(int enable_omega_autonomy) {
    g_cfg_autonomy = enable_omega_autonomy ? 1 : 0;
}

void cos_autonomy_bind_for_introspect(struct cos_autonomy_state *as) {
    g_bind = as;
}

int cos_autonomy_init(struct cos_autonomy_state *as) {
    if (as == NULL)
        return -1;
    memset(as, 0, sizeof(*as));
    as->mode                = COS_AUTONOMY_IDLE;
    as->prev_mode           = COS_AUTONOMY_IDLE;
    as->autonomous_since_ms = aut_now_ms();
    as->productivity        = 0.5f;
    as->inited              = 1;
    as->phase_ticks         = 0;
    cos_autonomy_bind_for_introspect(as);
    (void)cos_curiosity_init();
    return 0;
}

int cos_autonomy_transition(struct cos_autonomy_state *as, int new_mode,
                            const char *reason) {
    if (as == NULL)
        return -1;
    if (new_mode < COS_AUTONOMY_IDLE || new_mode > COS_AUTONOMY_SERVING)
        return -1;
    if (new_mode == COS_AUTONOMY_SERVING)
        as->prev_mode = as->mode;
    as->mode       = new_mode;
    as->phase_ticks = 0;
    if (reason != NULL)
        snprintf(as->last_reason, sizeof as->last_reason, "%s", reason);
    return 0;
}

int cos_autonomy_decide(struct cos_autonomy_state *as, char *next_action,
                        int max_len) {
    struct cos_curiosity_target pool[4];
    int                         n = 0;

    if (as == NULL || next_action == NULL || max_len <= 0)
        return -1;
    next_action[0] = '\0';
    as->next_hint[0] = '\0';

    switch (as->mode) {
    case COS_AUTONOMY_IDLE:
        if (cos_curiosity_rank(pool, 3, &n) == 0 && n > 0) {
            snprintf(next_action, (size_t)max_len,
                     "pick curiosity target (top score %.4f on %s)",
                     (double)pool[0].curiosity_score, pool[0].topic);
            snprintf(as->next_hint, sizeof as->next_hint,
                     "next: learn \"%s\" (σ=%.2f)", pool[0].topic,
                     (double)pool[0].sigma_mean);
        } else {
            snprintf(next_action, (size_t)max_len, "idle — no semantic gaps");
        }
        break;
    case COS_AUTONOMY_LEARNING:
        snprintf(next_action, (size_t)max_len,
                 "web learn \"%s\" (domain 0x%016llx)",
                 as->current_target.topic,
                 (unsigned long long)as->current_target.domain_hash);
        snprintf(as->next_hint, sizeof as->next_hint,
                 "explore angles on \"%s\" if σ stalls", as->current_target.topic);
        break;
    case COS_AUTONOMY_EXPLORING:
        snprintf(
            next_action, (size_t)max_len,
            "think: What aspects of %s am I missing? (σ-driven explore)",
            as->current_target.topic);
        snprintf(as->next_hint, sizeof as->next_hint,
                 "after: return to LEARNING with reframed query");
        break;
    case COS_AUTONOMY_PRACTICING:
        snprintf(next_action, (size_t)max_len,
                 "self-play / consolidation on known domains (σ<τ)");
        snprintf(as->next_hint, sizeof as->next_hint,
                 "then: pick a new high-σ topic when mastery holds");
        break;
    case COS_AUTONOMY_SERVING:
        snprintf(next_action, (size_t)max_len,
                 "user input — autonomous loop paused");
        break;
    default:
        snprintf(next_action, (size_t)max_len, "unknown mode");
        break;
    }
    return 0;
}

void cos_autonomy_report(const struct cos_autonomy_state *as) {
    char na[1024];
    if (as == NULL)
        return;
    cos_autonomy_decide((struct cos_autonomy_state *)as, na, (int)sizeof na);
    printf("Autonomy mode: %s\n", mode_label(as->mode));
    printf("  last transition: %s\n", as->last_reason[0] ? as->last_reason : "(none)");
    printf("  next action: %s\n", na);
    printf("  goals ok/fail/self: %d / %d / %d\n", as->goals_completed,
           as->goals_failed, as->self_generated_goals);
    printf("  productivity: %.3f  growth_rate(σ trend proxy): %.5f\n",
           (double)as->productivity, (double)as->growth_rate);
}

void cos_autonomy_note_learn(struct cos_autonomy_state *as, uint64_t domain_hash,
                             float sigma_before, float sigma_after) {
    if (as == NULL || !as->inited)
        return;
    as->last_learn_sigma_before = sigma_before;
    as->last_learn_sigma_after  = sigma_after;
    as->goals_completed += 1;
    (void)cos_curiosity_update(domain_hash, sigma_before, sigma_after);
    {
        int tot = as->goals_completed + as->goals_failed;
        as->productivity =
            tot > 0 ? (float)as->goals_completed / (float)tot : 0.5f;
    }
}

void cos_autonomy_omega_tick(struct cos_autonomy_state *as,
                             const struct cos_omega_state *omega_st,
                             float sigma_turn, const char *goal_text,
                             const char *output_text) {
    struct cos_curiosity_target pool[4];
    int                         n = 0;

    (void)sigma_turn;
    (void)goal_text;
    (void)output_text;
    if (as == NULL || !as->inited || !g_cfg_autonomy)
        return;
    as->growth_rate = omega_st != NULL ? omega_st->sigma_trend : 0.f;

    switch (as->mode) {
    case COS_AUTONOMY_IDLE:
        if (cos_curiosity_rank(pool, 3, &n) == 0 && n > 0) {
            as->current_target = pool[0];
            as->self_generated_goals++;
            cos_autonomy_transition(as, COS_AUTONOMY_LEARNING,
                                    "σ-ranked curiosity target");
        }
        break;
    case COS_AUTONOMY_LEARNING:
        if (as->phase_ticks > 25
            && as->last_learn_sigma_after + 0.02f
                   >= as->last_learn_sigma_before
            && as->last_learn_sigma_before > 1e-6f) {
            cos_autonomy_transition(as, COS_AUTONOMY_EXPLORING,
                                    "σ plateau — explore");
        } else if (as->current_target.sigma_mean < 0.12f) {
            cos_autonomy_transition(as, COS_AUTONOMY_PRACTICING,
                                    "local σ low — practice / widen");
        }
        break;
    case COS_AUTONOMY_EXPLORING:
        if (as->phase_ticks > 20)
            cos_autonomy_transition(as, COS_AUTONOMY_PRACTICING,
                                    "explore budget exhausted");
        break;
    case COS_AUTONOMY_PRACTICING:
        if (as->phase_ticks > 15)
            cos_autonomy_transition(as, COS_AUTONOMY_IDLE,
                                    "phase complete — pick new topic");
        break;
    case COS_AUTONOMY_SERVING:
        break;
    default:
        break;
    }

    if (as->mode != COS_AUTONOMY_IDLE && as->mode != COS_AUTONOMY_SERVING)
        as->phase_ticks++;
}

void cos_autonomy_introspect_print(FILE *fp) {
    struct cos_autonomy_state *as = g_bind;
    char                       na[1024];
    int64_t                    up_ms;
    int                        h, m;

    if (fp == NULL)
        return;
    fputs("\n+-- Creation OS Status (autonomy) --------------------------+\n",
          fp);
    if (as == NULL || !as->inited) {
        fputs("| Mode: (Ω-loop not running — no autonomy snapshot)          |\n",
              fp);
        fputs("+------------------------------------------------------------+\n",
              fp);
        return;
    }
    cos_autonomy_decide(as, na, (int)sizeof na);
    up_ms = aut_now_ms() - as->autonomous_since_ms;
    if (up_ms < 0)
        up_ms = 0;
    h = (int)(up_ms / 3600000LL);
    m = (int)((up_ms % 3600000LL) / 60000LL);
    fprintf(fp, "| Mode: %-8s (autonomous)                            |\n",
            mode_label(as->mode));
    fprintf(fp, "| Topic: %-50.50s |\n", as->current_target.topic[0]
                                                ? as->current_target.topic
                                                : "(none)");
    fprintf(fp, "| sigma domain: %.3f  learn sigma: %.3f -> %.3f          |\n",
            (double)as->current_target.sigma_mean,
            (double)as->last_learn_sigma_before,
            (double)as->last_learn_sigma_after);
    fprintf(fp, "| Uptime: %dh %dm autonomous                            |\n", h,
            m);
    fprintf(fp, "| Goals completed: %-5d  failed: %-5d  self-goals: %-5d   |\n",
            as->goals_completed, as->goals_failed, as->self_generated_goals);
    fprintf(fp, "| Growth (sigma trend proxy): %+8.5f                      |\n",
            (double)as->growth_rate);
    fprintf(fp, "| Next: %-52.52s |\n", as->next_hint[0] ? as->next_hint : na);
    fprintf(fp, "| Last reason: %-47.47s |\n",
            as->last_reason[0] ? as->last_reason : "—");
    fputs("| Curiosity queue:                                          |\n", fp);
    cos_curiosity_fprint_queue(fp, 3);
    fputs("+------------------------------------------------------------+\n",
          fp);
}

int cos_autonomy_self_test(void) {
    struct cos_autonomy_state st;
    char                      buf[256];
    if (cos_autonomy_init(&st) != 0)
        return 1;
    cos_autonomy_configure(1);
    if (cos_autonomy_decide(&st, buf, (int)sizeof buf) != 0)
        return 2;
    if (cos_autonomy_transition(&st, COS_AUTONOMY_LEARNING, "test") != 0)
        return 3;
    if (st.mode != COS_AUTONOMY_LEARNING)
        return 4;
    cos_autonomy_note_learn(&st, 99ULL, 0.8f, 0.5f);
    return 0;
}
