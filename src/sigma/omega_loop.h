/*
 * Ω-loop — unified perceive → think → gate → learn iteration.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_OMEGA_LOOP_H
#define COS_SIGMA_OMEGA_LOOP_H

#include <signal.h>
#include <stdint.h>
#include <stdio.h>

/** SIGINT/SIGTERM: checked each Ω turn (cos_omega_cli registers handlers). */
extern volatile sig_atomic_t cos_omega_signal_halt_requested;

#ifdef __cplusplus
extern "C" {
#endif

/** Ledger coherence bucket when σ session mean is elevated (see state_ledger.c). */
#define COS_OMEGA_COHERENCE_AT_RISK 2

struct cos_omega_config {
    int   max_turns;
    int   consolidate_interval;
    float sigma_halt_threshold;
    float k_eff_halt_threshold;
    int   human_review_interval;

    /** Wall-clock budget for cos_think_run + overhead (seconds). ≤0 → default/env. */
    int turn_timeout_s;
    /** Override adaptive compute; <0 = use spike budget. */
    int max_rethinks;

    int enable_ttt;
    int enable_search;
    int enable_codegen;
    int enable_federation;
    int enable_physical;
    int enable_consciousness;
    int simulation_mode;

    /** Idle web learning (cos learn) — spaced by idle_learn_cooldown_ms. */
    int       enable_learning;
    int       adapt_interval; /* turns between living-weight adaptation */
    int64_t   idle_learn_cooldown_ms;

    /** Curiosity + autonomy brain (COS_OMEGA_AUTONOMY=1). */
    int       enable_autonomy;

    /** EvoTest-style rule evolver (writes ~/.cos/omega/evolver.jsonl). */
    int       enable_evolver;
    int       evolver_interval; /* turns between steps; default 10 */
    int       enable_pattern_extract;
    int       pattern_interval; /* default 50 */
    int       autonomous_mode; /* rotate graded prompt bank as COS_OMEGA_GOAL */
    int       verbose_evolver;   /* stderr echo from evolver rules */
};

/** Per-turn fields for JSONL (not all duplicated in cos_omega_state). */
struct cos_omega_turn_emit {
    int64_t t_ms;
    float   sigma; /* σ proof this turn */
    int     attribution;
    int     gate_action;
    int     cache_hit;
    float   energy_turn_j;
    float   co2_turn_g;
    int64_t latency_ms;
    int     tokens_est; /* Ω think output length heuristic */
};

struct cos_omega_state {
    int   turn;
    float sigma_mean;
    float sigma_trend;
    float k_eff;
    int   consciousness_level;
    float energy_total_joules;
    float co2_total_grams;
    int   skills_learned;
    int   episodes_stored;
    int   cache_hits;
    int   rollbacks;
    int   pauses;

    float sigma_mean_initial;
    float k_eff_initial;

    int64_t started_ms;
    int64_t elapsed_ms;

    int running;
    int halt_requested;
    char halt_reason[256];
};

int cos_omega_init(const struct cos_omega_config *config,
                   struct cos_omega_state          *state);

int cos_omega_run(struct cos_omega_state *state);

int cos_omega_step(struct cos_omega_state *state);

int cos_omega_halt(struct cos_omega_state *state, const char *reason);

/** malloc'd UTF-8 report — caller free() when non-NULL. */
char *cos_omega_report(const struct cos_omega_state *state);

void cos_omega_print_status(const struct cos_omega_state *state);

/** Persist ledger, episodic consolidate, write ~/.cos/omega/last_report.txt, print report. */
int cos_omega_finish_session(struct cos_omega_state *state);

int cos_omega_self_test(void);

/** Append one JSONL line for a completed turn; fflush caller. */
int cos_omega_emit_event(FILE                          *fp,
                         const struct cos_omega_state   *state,
                         int                             turn_completed,
                         const struct cos_omega_turn_emit *row);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_OMEGA_LOOP_H */
