/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v148 σ-Sovereign — implementation.
 */
#include "sovereign.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* SplitMix64 — shared PRNG spec with the rest of the sovereign
 * stack so v148 traces replay byte-identically. */
static uint64_t splitmix(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float urand(uint64_t *s) {
    uint64_t x = splitmix(s);
    return (float)((x >> 40) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

/* Rotating weakest-topic roster used for the IDENTIFY stage.
 * In v148.1 this is replaced by a live v141 curriculum query. */
static const char *ROSTER[5] = {
    "history", "math", "language", "code", "logic"
};

/* Difficulties matching v145's default roster (math=0.15, code=0.05,
 * history=0.40, language=0.22, logic=0.10) so auto-acquire behaves
 * like a "real" curriculum step. */
static float roster_difficulty(const char *topic) {
    if (!strcmp(topic, "math"))     return 0.15f;
    if (!strcmp(topic, "code"))     return 0.05f;
    if (!strcmp(topic, "history"))  return 0.40f;
    if (!strcmp(topic, "language")) return 0.22f;
    if (!strcmp(topic, "logic"))    return 0.10f;
    return 0.30f;
}

void cos_v148_default_config(cos_v148_config_t *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    out->mode               = COS_V148_SUPERVISED;
    out->tau_sovereign      = 0.10f;    /* σ_rsi upper bound      */
    out->tau_skill_install  = COS_V145_TAU_INSTALL;
    out->tau_skill_share    = COS_V145_TAU_SHARE;
    out->tau_genesis_merge  = COS_V146_TAU_MERGE;
    out->seed               = 0x501EC08ULL;
    out->starting_score     = 0.50f;
}

void cos_v148_init(cos_v148_state_t *s, const cos_v148_config_t *cfg) {
    if (!s) return;
    memset(s, 0, sizeof *s);
    if (cfg) s->cfg = *cfg; else cos_v148_default_config(&s->cfg);
    cos_v144_init(&s->rsi, s->cfg.starting_score);
    cos_v145_library_init(&s->library);
    cos_v146_init(&s->genesis);
    s->prng = s->cfg.seed ^ 0xA11DEC0DEULL;
}

/* Produce a measured score for the current cycle.  Trajectory is
 * a slowly-improving walk with bounded noise, so on the default
 * seed v144 accepts most submits and σ_rsi stays below
 * τ_sovereign.  The bias floor (0.48) keeps us well below 1.0 so
 * the v144 clamp doesn't dominate the signal. */
static float synth_score(cos_v148_state_t *s) {
    float r = urand(&s->prng);                 /* [0, 1)             */
    float noise = (r - 0.5f) * 0.08f;
    float drift = 0.015f * (float)(s->cycles_run + 1);
    float base  = s->rsi.current_score + drift + noise;
    if (base < 0.02f) base = 0.02f;
    if (base > 0.98f) base = 0.98f;
    return base;
}

void cos_v148_emergency_stop(cos_v148_state_t *s) {
    if (!s) return;
    s->emergency_stopped = 1;
}

void cos_v148_resume_from_emergency(cos_v148_state_t *s) {
    if (!s) return;
    s->emergency_stopped = 0;
}

int cos_v148_approve_all(cos_v148_state_t *s) {
    if (!s) return -1;
    int n = 0;
    for (int i = 0; i < s->genesis.n_candidates; ++i) {
        if (s->genesis.candidates[i].status == COS_V146_PENDING) {
            if (cos_v146_approve(&s->genesis, i) == 0) n++;
        }
    }
    return n;
}

int cos_v148_cycle(cos_v148_state_t *s, cos_v148_cycle_report_t *out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    out->cycle_index = s->cycles_run;
    out->mode        = (int)s->cfg.mode;

    if (s->emergency_stopped) {
        out->emergency_stopped = 1;
        return 0;
    }

    /* ---------- Stage 1: MEASURE ----------------------------- */
    float score = synth_score(s);
    out->measured_score = score;

    cos_v144_cycle_report_t rsi_rep;
    cos_v144_submit(&s->rsi, score, &rsi_rep);
    out->rsi_accepted          = rsi_rep.accepted;
    out->rsi_rolled_back       = rsi_rep.rolled_back;
    out->rsi_paused_for_review = s->rsi.paused;
    out->sigma_rsi             = rsi_rep.sigma_rsi;
    s->n_actions_total++;

    /* ---------- Gate G1: instability halt -------------------- */
    if (rsi_rep.sigma_rsi > s->cfg.tau_sovereign) {
        out->unstable_halt = 1;
        s->unstable_halts++;
        s->cycles_run++;
        return 0;
    }

    /* ---------- Stage 2: IDENTIFY ---------------------------- */
    const char *topic = ROSTER[s->cycles_run % 5];
    safe_copy(out->weakness_topic, sizeof out->weakness_topic, topic);
    out->weakness_reported = 1;
    s->n_actions_total++;

    /* ---------- Stage 3: IMPROVE (skill acquire) ------------- */
    {
        float difficulty = roster_difficulty(topic);
        char name[48];
        snprintf(name, sizeof name, "%s_cycle_%d",
                 topic, s->cycles_run);
        uint64_t seed = s->cfg.seed ^
                        ((uint64_t)s->cycles_run * 0x9E3779B1ULL);
        int prior = s->library.n_skills;
        int rc = cos_v145_acquire(&s->library, name, topic,
                                  difficulty, seed,
                                  s->cfg.tau_skill_install, NULL);
        out->skill_generated    = 1;
        out->skill_installed    = (rc == 0) ? 1 : 0;
        out->skill_count_after  = s->library.n_skills;
        (void)prior;
        s->n_actions_total++;
    }

    /* ---------- Stage 4: EVOLVE (genesis propose) ------------ *
     * Propose a new kernel skeleton every 2nd cycle to keep the
     * genesis pressure tractable for the merge-gate. */
    if ((s->cycles_run % 2) == 0 &&
        s->genesis.n_candidates < COS_V146_MAX_CANDIDATES) {
        int version = 150 + s->cycles_run;
        char kname[40];
        snprintf(kname, sizeof kname, "GeneratedKernel_%d", version);
        char gap[80];
        snprintf(gap, sizeof gap,
                 "topic %s under-covered at cycle %d",
                 topic, s->cycles_run);
        uint64_t gseed = s->cfg.seed ^
                         ((uint64_t)s->cycles_run * 0xBF58476DULL);
        int gi = cos_v146_generate(&s->genesis, version, kname, gap,
                                   gseed, s->cfg.tau_genesis_merge,
                                   NULL, 0);
        if (gi >= 0) {
            out->kernel_generated   = 1;
            out->kernel_status      = (int)s->genesis.candidates[gi].status;
            out->kernel_sigma_code  = s->genesis.candidates[gi].sigma_code;

            /* Autonomous mode auto-approves pending candidates. */
            if (s->cfg.mode == COS_V148_AUTONOMOUS &&
                s->genesis.candidates[gi].status == COS_V146_PENDING) {
                cos_v146_approve(&s->genesis, gi);
                out->kernel_status =
                    (int)s->genesis.candidates[gi].status;
            }
        }
        s->n_actions_total++;
    }

    /* ---------- Stage 5: REFLECT ----------------------------- *
     * Canonical 4-step reasoning trace whose weakest link is
     * always step 2 (σ=0.67).  v148.1 hooks this up to v111.2
     * live traces. */
    {
        cos_v147_trace_t tr;
        cos_v147_trace_init(&tr, "42", 0.22f);
        cos_v147_trace_add(&tr, "parse question",  0.15f);
        cos_v147_trace_add(&tr, "recall formula",  0.22f);
        cos_v147_trace_add(&tr, "apply with values", 0.67f);
        cos_v147_trace_add(&tr, "verify",          0.18f);
        cos_v147_reflection_t rr;
        cos_v147_reflect(&tr, "42", 0.22f, &rr);
        out->reflect_weakest_step  = rr.weakest_step;
        out->reflect_weakest_sigma = rr.weakest_sigma;
        out->reflect_consistent    = rr.consistency_ok;
        s->n_actions_total++;
    }

    /* ---------- Stage 6: SHARE (mesh sweep) ------------------ */
    out->n_shared_after_sweep =
        cos_v145_share(&s->library, s->cfg.tau_skill_share);
    s->n_actions_total++;

    /* ---------- G2: pending approvals in supervised mode ----- */
    if (s->cfg.mode == COS_V148_SUPERVISED) {
        int pending = 0;
        for (int i = 0; i < s->genesis.n_candidates; ++i) {
            if (s->genesis.candidates[i].status == COS_V146_PENDING)
                pending++;
        }
        out->pending_approvals = pending;
    } else {
        out->pending_approvals = 0;
    }

    s->cycles_run++;
    return 0;
}

int cos_v148_cycle_to_json(const cos_v148_cycle_report_t *r,
                           char *buf, size_t cap) {
    if (!r || !buf || cap == 0) return -1;
    int rc = snprintf(buf, cap,
        "{\"cycle\":%d,\"mode\":%d,"
        "\"measured_score\":%.6f,\"rsi_accepted\":%s,"
        "\"rsi_rolled_back\":%s,\"rsi_paused_for_review\":%s,"
        "\"sigma_rsi\":%.6f,"
        "\"weakness_topic\":\"%s\","
        "\"skill_generated\":%s,\"skill_installed\":%s,"
        "\"skill_count_after\":%d,"
        "\"kernel_generated\":%s,\"kernel_status\":%d,"
        "\"kernel_sigma_code\":%.6f,"
        "\"reflect_weakest_step\":%d,\"reflect_weakest_sigma\":%.6f,"
        "\"reflect_consistent\":%s,"
        "\"n_shared_after_sweep\":%d,"
        "\"pending_approvals\":%d,\"emergency_stopped\":%s,"
        "\"unstable_halt\":%s}",
        r->cycle_index, r->mode,
        (double)r->measured_score,
        r->rsi_accepted          ? "true" : "false",
        r->rsi_rolled_back       ? "true" : "false",
        r->rsi_paused_for_review ? "true" : "false",
        (double)r->sigma_rsi,
        r->weakness_topic,
        r->skill_generated ? "true" : "false",
        r->skill_installed ? "true" : "false",
        r->skill_count_after,
        r->kernel_generated ? "true" : "false",
        r->kernel_status,
        (double)r->kernel_sigma_code,
        r->reflect_weakest_step,
        (double)r->reflect_weakest_sigma,
        r->reflect_consistent ? "true" : "false",
        r->n_shared_after_sweep,
        r->pending_approvals,
        r->emergency_stopped ? "true" : "false",
        r->unstable_halt     ? "true" : "false");
    if (rc < 0 || (size_t)rc >= cap) return -1;
    return rc;
}

int cos_v148_state_to_json(const cos_v148_state_t *s,
                           char *buf, size_t cap) {
    if (!s || !buf || cap == 0) return -1;
    int rc = snprintf(buf, cap,
        "{\"v148_state\":true,\"cycles_run\":%d,"
        "\"mode\":%d,\"seed\":%llu,"
        "\"emergency_stopped\":%s,\"unstable_halts\":%d,"
        "\"n_actions_total\":%d,"
        "\"rsi_accepted\":%d,\"rsi_rolled_back\":%d,"
        "\"rsi_current_score\":%.6f,\"sigma_rsi\":%.6f,"
        "\"skills\":%d,\"skills_rejected\":%d,"
        "\"kernels_generated\":%d,\"kernels_approved\":%d,"
        "\"kernels_pending\":%d,\"kernels_gated_out\":%d}",
        s->cycles_run, (int)s->cfg.mode,
        (unsigned long long)s->cfg.seed,
        s->emergency_stopped ? "true" : "false",
        s->unstable_halts,
        s->n_actions_total,
        s->rsi.accepted_cycles, s->rsi.rolled_back_cycles,
        (double)s->rsi.current_score,
        (double)cos_v144_sigma_rsi(&s->rsi),
        s->library.n_skills, s->library.n_rejected,
        s->genesis.n_candidates, s->genesis.n_approved,
        s->genesis.n_candidates - s->genesis.n_approved
            - s->genesis.n_rejected - s->genesis.n_gated_out,
        s->genesis.n_gated_out);
    if (rc < 0 || (size_t)rc >= cap) return -1;
    return rc;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v148 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v148_self_test(void) {
    /* --- A. Supervised: 6 cycles -------------------------------- *
     * Expect: rsi accepts most submits, library installs several
     * skills, genesis has ≥ 1 candidate PENDING, every cycle
     * reports reflect_weakest_step=2.                              */
    cos_v148_config_t cfg;
    cos_v148_default_config(&cfg);
    cfg.mode = COS_V148_SUPERVISED;

    cos_v148_state_t s;
    cos_v148_init(&s, &cfg);
    _CHECK(s.rsi.current_score == 0.50f,  "baseline seeded");

    for (int k = 0; k < 6; ++k) {
        cos_v148_cycle_report_t r;
        _CHECK(cos_v148_cycle(&s, &r) == 0, "cycle ok");
        _CHECK(r.weakness_reported == 1,    "weakness reported");
        _CHECK(r.skill_generated   == 1,    "skill attempted");
        _CHECK(r.reflect_weakest_step == 2,
               "canonical reflection weakest step");
        _CHECK(r.reflect_consistent == 1,   "reflect consistent");
        _CHECK(r.emergency_stopped == 0,    "not emergency stopped");
    }
    fprintf(stderr,
        "check-v148: supervised 6 cycles → skills=%d, kernels=%d, "
        "pending=%d, σ_rsi=%.4f\n",
        s.library.n_skills, s.genesis.n_candidates,
        s.genesis.n_candidates - s.genesis.n_approved,
        (double)cos_v144_sigma_rsi(&s.rsi));
    _CHECK(s.cycles_run == 6,               "6 cycles executed");
    _CHECK(s.library.n_skills >= 3,         "≥ 3 skills installed");
    _CHECK(s.genesis.n_candidates >= 1,     "≥ 1 kernel generated");

    /* At least one kernel should be PENDING in supervised mode. */
    int supervised_pending = 0;
    for (int i = 0; i < s.genesis.n_candidates; ++i)
        if (s.genesis.candidates[i].status == COS_V146_PENDING)
            supervised_pending++;
    _CHECK(supervised_pending >= 1,
           "supervised mode leaves ≥ 1 candidate PENDING");
    _CHECK(s.genesis.n_approved == 0,
           "supervised mode does not auto-approve");

    /* --- B. Emergency stop + resume ---------------------------- */
    cos_v148_emergency_stop(&s);
    cos_v148_cycle_report_t rstop;
    cos_v148_cycle(&s, &rstop);
    _CHECK(rstop.emergency_stopped == 1,
           "cycle short-circuited after emergency_stop");
    _CHECK(s.cycles_run == 6,
           "emergency-stopped cycle did not bump cycles_run");
    cos_v148_resume_from_emergency(&s);
    _CHECK(s.emergency_stopped == 0, "resume clears emergency stop");
    _CHECK(cos_v148_cycle(&s, &rstop) == 0, "cycle resumes");
    _CHECK(rstop.emergency_stopped == 0, "post-resume cycle runs");
    _CHECK(s.cycles_run == 7, "cycles_run advances after resume");

    /* --- C. Autonomous mode auto-approves --------------------- */
    cos_v148_config_t cfg_a;
    cos_v148_default_config(&cfg_a);
    cfg_a.mode = COS_V148_AUTONOMOUS;
    cos_v148_state_t a;
    cos_v148_init(&a, &cfg_a);
    for (int k = 0; k < 6; ++k) {
        cos_v148_cycle_report_t r;
        _CHECK(cos_v148_cycle(&a, &r) == 0, "autonomous cycle");
    }
    fprintf(stderr,
        "check-v148: autonomous 6 cycles → skills=%d, kernels=%d approved=%d\n",
        a.library.n_skills, a.genesis.n_candidates,
        a.genesis.n_approved);
    _CHECK(a.genesis.n_approved >= 1,
           "autonomous mode auto-approves ≥ 1 candidate");

    /* --- D. Unstable halt (forcibly drop τ_sovereign low) ----- */
    cos_v148_config_t cfg_u;
    cos_v148_default_config(&cfg_u);
    cfg_u.tau_sovereign = 1e-9f;     /* virtually any noise trips it */
    cos_v148_state_t u;
    cos_v148_init(&u, &cfg_u);
    for (int k = 0; k < 5; ++k) {
        cos_v148_cycle_report_t r;
        cos_v148_cycle(&u, &r);
    }
    _CHECK(u.unstable_halts >= 1,
           "σ_rsi > τ_sovereign triggers unstable_halt at least once");

    /* --- E. JSON shape ---------------------------------------- */
    char js[2048];
    int jw = cos_v148_state_to_json(&s, js, sizeof js);
    _CHECK(jw > 0,                              "state json emit");
    _CHECK(strstr(js, "\"v148_state\":true") != NULL, "tag");
    _CHECK(strstr(js, "\"skills\":") != NULL,   "skills field");

    fprintf(stderr,
        "check-v148: OK (supervised + autonomous + emergency stop + unstable halt)\n");
    return 0;
}
