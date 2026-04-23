/*
 * Ω-loop — unified runtime iteration over existing σ modules.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "omega_loop.h"

#include "coherence_watchdog.h"
#include "adaptive_compute.h"
#include "awareness_log.h"
#include "consciousness_meter.h"
#include "embodiment.h"
#include "energy_accounting.h"
#include "engram_episodic.h"
#include "error_attribution.h"
#include "evolution_engine.h"
#include "federation.h"
#include "green_score.h"
#include "inference_cache.h"
#include "knowledge_graph.h"
#include "mission.h"
#include "perception.h"
#include "proof_receipt.h"
#include "reinforce.h"
#include "skill_distill.h"
#include "speculative_sigma.h"
#include "spike_engine.h"
#include "ttt_runtime.h"
#include "sovereign_limits.h"
#include "state_ledger.h"
#include "world_model.h"

#include "../cli/cos_think.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

volatile sig_atomic_t cos_omega_signal_halt_requested;

static struct cos_omega_config      g_cfg;
static int                           g_inited;
static struct cos_state_ledger       g_ledger;
static struct cos_spike_engine       g_spike;
static struct cos_embodiment         g_embod;
static struct cos_consciousness_state  g_conscious;
static struct cos_mission            g_mission;
static struct cos_sovereign_state    g_sov;
static struct cos_think_result         g_last_tr;
static struct cos_federation_config    g_fed_cfg;
static int                           g_have_perception;
static int                           g_have_kg;
static int                           g_have_cache;
static int                           g_have_federation;
static float                         g_prev_conscious_sigma = 0.5f;

static FILE                         *g_omega_events_fp;
static float                         g_omega_prev_energy_j;
static float                         g_omega_prev_co2_g;
static int                           g_omega_sigma_streak;
static int                           g_omega_cache_info_done;

static int64_t omega_now_ms(void)
{
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000LL;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
#elif defined(__APPLE__)
    static mach_timebase_info_data_t tb;
    uint64_t                         t = mach_absolute_time();
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return (int64_t)(t * tb.numer / tb.denom / 1000000ULL);
#else
    return (int64_t)time(NULL) * 1000LL;
#endif
}

static void omega_skip(const char *module)
{
    fprintf(stderr, "[omega] module %s not available, skipping\n", module);
}

static void omega_cfg_defaults(struct cos_omega_config *c)
{
    memset(c, 0, sizeof(*c));
    c->max_turns              = 0;
    c->consolidate_interval   = 50;
    c->sigma_halt_threshold   = 0.96f;
    c->k_eff_halt_threshold   = 0.07f;
    c->human_review_interval  = 0;
    c->turn_timeout_s         = 0; /* cos_omega_init: 120 or COS_OMEGA_TURN_TIMEOUT_S */
    c->max_rethinks           = -1;
    c->enable_ttt             = 1;
    c->enable_search          = 1;
    c->enable_codegen         = 0;
    c->enable_federation      = 0;
    c->enable_physical        = 0;
    c->enable_consciousness   = 1;
    c->simulation_mode        = 0;
}

static int omega_home_subdir(char *buf, size_t cap, const char *leaf)
{
    const char *h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";
    return snprintf(buf, cap, "%s/.cos/omega/%s", h, leaf);
}

static int omega_ensure_dir(void)
{
    char b[512];
    const char *h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";
    snprintf(b, sizeof b, "%s/.cos/omega", h);
    return mkdir(b, 0700) == 0 || errno == EEXIST ? 0 : -1;
}

static int omega_halt_file_present(void)
{
    char p[512];
    if (omega_home_subdir(p, sizeof p, "halt") >= (int)sizeof p)
        return 0;
    return access(p, F_OK) == 0;
}

static void omega_consume_halt_file(void)
{
    char p[512];
    if (omega_home_subdir(p, sizeof p, "halt") >= (int)sizeof p)
        return;
    (void)unlink(p);
}

static int omega_events_path(char *buf, size_t cap)
{
    return omega_home_subdir(buf, cap, "events.jsonl");
}

static const char *omega_action_json(int a)
{
    if (a == COS_SIGMA_ACTION_RETHINK)
        return "RETHINK";
    if (a == COS_SIGMA_ACTION_ABSTAIN)
        return "ABSTAIN";
    return "ACCEPT";
}

static void omega_json_escape_alert(FILE *fp, const char *msg)
{
    const char *p;
    if (!msg)
        return;
    for (p = msg; *p; ++p) {
        if (*p == '"' || *p == '\\')
            fputc('\\', fp);
        fputc((unsigned char)*p, fp);
    }
}

static void omega_emit_alert_json(FILE *fp, int64_t t_ms, const char *msg,
                                   const char *level)
{
    if (!fp || !msg || !level)
        return;
    fprintf(fp, "{\"t\":%lld,\"alert\":\"", (long long)t_ms);
    omega_json_escape_alert(fp, msg);
    fprintf(fp, "\",\"level\":\"%s\"}\n", level);
}

static void omega_run_alert_rules(FILE *fp, const struct cos_omega_state *st,
                                  float sigma_turn, float energy_turn_j)
{
    int64_t       tnow = omega_now_ms();
    float         ratio;

    if (sigma_turn > 0.7f)
        g_omega_sigma_streak++;
    else
        g_omega_sigma_streak = 0;

    if (g_omega_sigma_streak >= 3)
        omega_emit_alert_json(
            fp, tnow,
            "sigma above 0.7 for 3 consecutive turns", "ALERT");

    if (st->k_eff < 0.3f)
        omega_emit_alert_json(fp, tnow, "k_eff below 0.3", "ALERT");

    if (energy_turn_j > 1.0f)
        omega_emit_alert_json(fp, tnow,
                              "energy per turn exceeds 1 J", "WARNING");

    if (st->turn >= 50 && !g_omega_cache_info_done && st->turn > 0) {
        ratio = (float)st->cache_hits / (float)st->turn;
        if (ratio < 0.10f) {
            omega_emit_alert_json(
                fp, tnow,
                "cache hit ratio below 10pct after 50+ turns", "INFO");
            g_omega_cache_info_done = 1;
        }
    }
}

static int omega_write_status(struct cos_omega_state *st)
{
    FILE *f;
    char  path[512];

    if (omega_ensure_dir() != 0)
        return -1;
    if (omega_home_subdir(path, sizeof path, "status.txt") >= (int)sizeof path)
        return -1;
    f = fopen(path, "w");
    if (!f)
        return -1;
    fprintf(f,
            "turn=%d\nsigma_mean=%.6f\nk_eff=%.6f\nconsciousness_level=%d\n"
            "elapsed_ms=%lld\nrunning=%d\nhalt=%d\n",
            st->turn, (double)st->sigma_mean, (double)st->k_eff,
            st->consciousness_level, (long long)st->elapsed_ms, st->running,
            st->halt_requested);
    fclose(f);
    return 0;
}

static const char *omega_pick_goal(struct cos_omega_state *st)
{
    static const char *kGoals[] = {
        "Ω-loop turn: verify σ-state ledger coherence",
        "Ω-loop turn: semantic cache + skill routing probe",
        "Ω-loop turn: episodic store + consolidation path",
        "Ω-loop turn: speculative σ + spike gating sample",
    };
    const char *genv = getenv("COS_OMEGA_GOAL");

    if (genv && genv[0])
        return genv;
    return kGoals[(unsigned)st->turn % (sizeof kGoals / sizeof kGoals[0])];
}

static int omega_estimate_tokens(const struct cos_think_result *tr)
{
    int n = 0;
    int i;
    if (!tr || tr->n_subtasks <= 0)
        return 16;
    for (i = 0; i < tr->n_subtasks; ++i)
        n += (int)strlen(tr->subtasks[i].answer) / 4;
    if (n < 1)
        n = 1;
    return n;
}

static void omega_embed_physical_if_enabled(struct cos_omega_state *st,
                                            const char           *goal)
{
    struct cos_action            act;
    struct cos_physical_state    pred;

    if (!g_cfg.enable_physical || g_cfg.simulation_mode)
        return;
    memset(&act, 0, sizeof act);
    memset(&pred, 0, sizeof pred);
    if (cos_embodiment_plan_action(&g_embod, goal, &act) != 0) {
        omega_skip("embodiment_plan_action");
        return;
    }
    if (cos_embodiment_simulate(&g_embod, &act, &pred) != 0) {
        omega_skip("embodiment_simulate");
        return;
    }
    {
        float gap =
            cos_embodiment_sim_to_real_gap(&pred, &g_embod.state);
        if (gap > 0.3f) {
            if (cos_mission_rollback(&g_mission) == 0)
                st->rollbacks++;
        }
    }
}

static void omega_consolidate_turn(struct cos_omega_state *st)
{
    struct cos_skill sk;

    if (st->turn <= 0 || st->turn % g_cfg.consolidate_interval != 0)
        return;

    if (cos_engram_consolidate(g_cfg.consolidate_interval) != 0)
        omega_skip("engram_consolidate");

    memset(&sk, 0, sizeof sk);
    if (g_last_tr.n_subtasks > 0
        && cos_skill_distill(&g_last_tr, &sk) == 0) {
        if (cos_skill_save(&sk) == 0)
            st->skills_learned++;
    }

    if (g_cfg.enable_codegen) {
        struct cos_evolution_generation eg[1];

        memset(eg, 0, sizeof eg);
        if (cos_evolution_run(1, eg) != 0)
            omega_skip("evolution_engine");
    }

    if (g_cfg.enable_federation && g_have_federation) {
        struct cos_federation_update u;

        memset(&u, 0, sizeof u);
        u.update_type   = COS_FED_UPDATE_TAU;
        u.domain_hash   = cos_engram_prompt_hash(g_last_tr.goal);
        u.tau_local     = 1.0f - st->sigma_mean;
        u.sigma_mean    = st->sigma_mean;
        u.sample_count  = st->turn;
        u.trust_score   = 0.9f;
        u.timestamp_ms  = omega_now_ms();
        if (cos_federation_share_update(&u) != 0)
            omega_skip("federation_share_update");
    }
}

int cos_omega_init(const struct cos_omega_config *config,
                   struct cos_omega_state          *state)
{
    struct cos_omega_config def;

    if (!state)
        return -1;

    omega_cfg_defaults(&def);
    memcpy(&g_cfg, &def, sizeof g_cfg);
    if (config)
        memcpy(&g_cfg, config, sizeof g_cfg);
    if (g_cfg.turn_timeout_s <= 0) {
        const char *te = getenv("COS_OMEGA_TURN_TIMEOUT_S");
        int          v  = (te != NULL && te[0] != '\0') ? atoi(te) : 0;
        g_cfg.turn_timeout_s = (v > 0) ? v : 120;
    }
    if (g_cfg.consolidate_interval <= 0)
        g_cfg.consolidate_interval = 50;
    if (g_cfg.sigma_halt_threshold <= 0.f)
        g_cfg.sigma_halt_threshold = 0.96f;
    if (g_cfg.k_eff_halt_threshold <= 0.f)
        g_cfg.k_eff_halt_threshold = 0.07f;

    memset(state, 0, sizeof(*state));
    state->running            = 1;
    state->sigma_mean_initial = -1.f;
    state->k_eff_initial      = -1.f;
    state->started_ms         = omega_now_ms();

    memset(&g_last_tr, 0, sizeof g_last_tr);
    memset(&g_mission, 0, sizeof g_mission);

    cos_state_ledger_init(&g_ledger);
    cos_spike_engine_init(&g_spike);
    cos_consciousness_init(&g_conscious);
    cos_sovereign_init(NULL);

    (void)cos_energy_init(NULL);

    g_have_perception = (cos_perception_init() == 0);
    if (!g_have_perception)
        omega_skip("perception_init");

    g_have_kg = (cos_kg_init() == 0);
    if (!g_have_kg)
        omega_skip("knowledge_graph_init");

    g_have_cache = (cos_inference_cache_init(4096) == 0);
    if (!g_have_cache)
        omega_skip("inference_cache_init");

    cos_skill_begin_session();

    if (g_cfg.enable_consciousness && cos_awareness_open() != 0)
        omega_skip("awareness_open");

    if (cos_embodiment_init(&g_embod, g_cfg.simulation_mode
                                      || g_cfg.enable_physical)
        != 0)
        omega_skip("embodiment_init");

    {
        const char *prev = getenv("COS_MISSION_OFFLINE");
        char        prevbuf[8];

        if (prev)
            snprintf(prevbuf, sizeof prevbuf, "%s", prev);
        setenv("COS_MISSION_OFFLINE", "1", 1);
        if (cos_mission_create("Ω-loop", &g_mission) != 0)
            omega_skip("mission_create");
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
    }

    g_have_federation = 0;
    if (g_cfg.enable_federation) {
        if (cos_federation_default_config(&g_fed_cfg) == 0
            && cos_federation_init(&g_fed_cfg) == 0)
            g_have_federation = 1;
        else
            omega_skip("federation_init");
    }

    cos_omega_signal_halt_requested = 0;

    {
        char evpath[512];
        int  n = omega_events_path(evpath, sizeof evpath);
        if (g_omega_events_fp != NULL) {
            fclose(g_omega_events_fp);
            g_omega_events_fp = NULL;
        }
        g_omega_prev_energy_j   = 0.f;
        g_omega_prev_co2_g      = 0.f;
        g_omega_sigma_streak    = 0;
        g_omega_cache_info_done = 0;
        if (n > 0 && n < (int)sizeof evpath) {
            g_omega_events_fp = fopen(evpath, "a");
            if (g_omega_events_fp == NULL)
                omega_skip("events.jsonl open");
            else
                setvbuf(g_omega_events_fp, NULL, _IONBF, 0);
        }
    }

    g_inited                        = 1;
    return 0;
}

int cos_omega_halt(struct cos_omega_state *state, const char *reason)
{
    if (!state)
        return -1;
    state->halt_requested = 1;
    state->running        = 0;
    if (reason)
        snprintf(state->halt_reason, sizeof state->halt_reason, "%s", reason);
    else
        state->halt_reason[0] = '\0';
    return 0;
}

static int omega_step_inner(struct cos_omega_state *state)
{
    const char                     *goal = omega_pick_goal(state);
    struct cos_perception_input     pin;
    struct cos_perception_result    pr;
    float                           meta[4];
    uint64_t                        bsc[COS_INF_W];
    uint64_t                        domain_h;
    struct cos_speculative_sigma    spec;
    struct cos_compute_budget       bud;
    struct cos_inference_cache_entry ice;
    int                             cache_hit = 0;
    char                            output[4096];
    struct cos_think_result           tr;
    float                           sigma_proof;
    float                           schans[4];
    int                             gate_decision = COS_SIGMA_ACTION_ACCEPT;
    struct cos_error_attribution      ea;
    struct cos_proof_receipt          receipt;
    struct cos_engram_episode         ep;
    cos_energy_timer_t                et;
    float                             spvals[8];
    int                               fired[8];
    int                               nf = 0;
    int                               omega_turn_timed_out = 0;
    int64_t                           omega_turn_t0_ms = omega_now_ms();

    memset(&pin, 0, sizeof pin);
    memset(&pr, 0, sizeof pr);
    memset(&ice, 0, sizeof ice);
    memset(&tr, 0, sizeof tr);
    memset(output, 0, sizeof output);

    pin.modality   = COS_PERCEPTION_TEXT;
    pin.timestamp  = omega_now_ms();
    snprintf(pin.text, sizeof pin.text, "%s", goal);

    if (g_cfg.simulation_mode) {
        snprintf(pr.description, sizeof pr.description, "%s", goal);
        pr.sigma = 0.42f;
    } else if (g_have_perception) {
        if (cos_perception_process(&pin, &pr) != 0) {
            omega_skip("perception_process");
            snprintf(pr.description, sizeof pr.description, "%s", goal);
            pr.sigma = 0.48f;
        }
    } else {
        snprintf(pr.description, sizeof pr.description, "%s", goal);
        pr.sigma = 0.48f;
    }

    meta[0] = pr.sigma;
    meta[1] = pr.sigma;
    meta[2] = pr.sigma;
    meta[3] = pr.sigma;

    if (g_have_kg)
        (void)cos_kg_extract_and_store(pr.description);

    cos_inference_bsc_encode_prompt(goal, bsc);
    domain_h = cos_engram_prompt_hash(goal);

    spec = cos_predict_sigma(bsc, domain_h);
    cos_spike_fill_channels_from_speculative(spec.predicted_sigma,
                                             spec.confidence, spvals);
    (void)cos_spike_check(&g_spike, spvals, fired, &nf);

    bud = cos_allocate_compute(spec.predicted_sigma, &g_spike);
    if (!g_cfg.enable_search)
        bud.enable_search = 0;
    if (!g_cfg.enable_ttt)
        bud.enable_ttt = 0;
    if (g_cfg.max_rethinks >= 0)
        bud.max_rethinks = g_cfg.max_rethinks;

    if (spec.early_abstain) {
        fputs("[omega] speculative early ABSTAIN\n", stderr);
        snprintf(output, sizeof output, "[ABSTAIN]");
        sigma_proof     = spec.predicted_sigma;
        gate_decision   = COS_SIGMA_ACTION_ABSTAIN;
        cos_state_ledger_update(&g_ledger, sigma_proof, meta, gate_decision);
        printf("%s\n", output);
        goto common_tail;
    }

    if (g_have_cache && cos_inference_cache_lookup(bsc, COS_INF_W, &ice) == 0
        && ice.response[0]) {
        snprintf(output, sizeof output, "%s", ice.response);
        cache_hit = 1;
        state->cache_hits++;
        cos_state_ledger_note_cache_hit(&g_ledger);
        sigma_proof = ice.sigma;
        memset(&tr, 0, sizeof tr);
        snprintf(tr.goal, sizeof tr.goal, "%s", goal);
        tr.n_subtasks           = 1;
        tr.sigma_mean           = ice.sigma;
        tr.subtasks[0].sigma_combined = ice.sigma;
        snprintf(tr.subtasks[0].answer, sizeof tr.subtasks[0].answer, "%s",
                 ice.response);
        memcpy(&g_last_tr, &tr, sizeof tr);
        gate_decision = COS_SIGMA_ACTION_ACCEPT;
        goto after_think;
    }

    {
        float local_tau = cos_engram_get_local_tau(domain_h);
        cos_energy_timer_start(&et);

        if (g_cfg.simulation_mode) {
            snprintf(tr.goal, sizeof tr.goal, "%s", goal);
            tr.n_subtasks                 = 1;
            tr.sigma_mean                 = 0.38f;
            tr.best_subtask_idx           = 0;
            tr.subtasks[0].sigma_combined = 0.38f;
            tr.subtasks[0].final_action = COS_SIGMA_ACTION_ACCEPT;
            snprintf(tr.subtasks[0].answer, sizeof tr.subtasks[0].answer,
                     "[Ω-sim] ok — σ≈0.38 for `%s`", goal);
        } else {
            int64_t t0_wall = omega_now_ms();
            int     trc     = cos_think_run(goal, bud.max_rethinks, &tr);

            if (trc != 0) {
                omega_skip("cos_think_run");
                snprintf(tr.subtasks[0].answer, sizeof tr.subtasks[0].answer,
                         "[think error]");
                tr.sigma_mean                 = g_ledger.sigma_combined;
                tr.n_subtasks                 = 1;
                tr.subtasks[0].sigma_combined = tr.sigma_mean;
                tr.subtasks[0].final_action = COS_SIGMA_ACTION_ABSTAIN;
            }

            if (g_cfg.turn_timeout_s > 0) {
                int64_t elapsed_ms = omega_now_ms() - t0_wall;
                if (elapsed_ms > (int64_t)g_cfg.turn_timeout_s * 1000LL) {
                    fprintf(stderr,
                            "[omega] turn %d timed out after %llds\n",
                            state->turn + 1,
                            (long long)(elapsed_ms / 1000LL));
                    omega_turn_timed_out = 1;
                    snprintf(tr.goal, sizeof tr.goal, "%s", goal);
                    tr.n_subtasks       = 1;
                    tr.best_subtask_idx = 0;
                    tr.sigma_mean       = 1.0f;
                    tr.subtasks[0].sigma_combined = 1.0f;
                    tr.subtasks[0].final_action =
                        COS_SIGMA_ACTION_ABSTAIN;
                    snprintf(tr.subtasks[0].answer,
                             sizeof tr.subtasks[0].answer,
                             "[Ω turn timeout]");
                }
            }
        }

        {
            struct cos_energy_measurement em =
                cos_energy_timer_stop_and_measure(&et, omega_estimate_tokens(&tr));

            state->energy_total_joules += em.estimated_joules;
            state->co2_total_grams += em.co2_grams;
        }

        memcpy(&g_last_tr, &tr, sizeof tr);

        if (!omega_turn_timed_out && local_tau > 1e-6f && tr.sigma_mean > local_tau
            && g_cfg.enable_ttt) {
            struct cos_ttt_update tu;
            const char           *ans =
                tr.subtasks[tr.best_subtask_idx >= 0 ? tr.best_subtask_idx : 0]
                    .answer;

            memset(&tu, 0, sizeof tu);
            if (cos_ttt_extract_signal(ans, NULL, 0, &tu) == 0)
                (void)cos_ttt_apply_fast_weights(&tu);
            if (cos_think_run(goal, 1, &tr) == 0)
                memcpy(&g_last_tr, &tr, sizeof tr);
            if (tr.sigma_mean > local_tau) {
                cos_mission_pause(&g_mission, "σ above τ after TTT");
                state->pauses++;
            }
        }

        snprintf(output, sizeof output, "%s",
                 tr.subtasks[tr.best_subtask_idx >= 0 ? tr.best_subtask_idx : 0]
                     .answer);
        sigma_proof = tr.sigma_mean;
        gate_decision =
            tr.subtasks[tr.best_subtask_idx >= 0 ? tr.best_subtask_idx : 0]
                .final_action;

        ea = cos_error_attribute(tr.sigma_mean, tr.sigma_mean, tr.sigma_mean,
                                 meta[0]);

        omega_embed_physical_if_enabled(state, goal);

        cos_sovereign_snapshot_state(&g_sov);
        (void)cos_sovereign_check(&g_sov, COS_SOVEREIGN_ACTION_GENERIC);
        cos_sovereign_snapshot_state(&g_sov);
        if (g_sov.human_required) {
            cos_mission_pause(&g_mission, "sovereign brake");
            state->pauses++;
        }
    }

after_think:
    cos_state_ledger_update(&g_ledger, sigma_proof, meta, gate_decision);

    printf("%s\n", output);

common_tail:
    schans[0] = sigma_proof;
    schans[1] = sigma_proof;
    schans[2] = sigma_proof;
    schans[3] = sigma_proof;

    memset(&receipt, 0, sizeof receipt);
    ea = cos_error_attribute(sigma_proof, sigma_proof, sigma_proof, meta[0]);
    if (cos_proof_receipt_generate(output, sigma_proof, schans, meta,
                                   gate_decision, &ea, 0,
                                   &receipt)
        != 0)
        omega_skip("proof_receipt_generate");

    memset(&ep, 0, sizeof ep);
    ep.timestamp_ms     = omega_now_ms();
    ep.prompt_hash      = cos_engram_prompt_hash(goal);
    ep.sigma_combined   = sigma_proof;
    ep.action           = gate_decision;
    ep.was_correct      = (gate_decision == COS_SIGMA_ACTION_ACCEPT);
    ep.attribution      = ea.source;
    ep.ttt_applied      = g_cfg.enable_ttt ? 1 : 0;
    ep.turn_timeout     = omega_turn_timed_out;

    if (cos_engram_episode_store(&ep) == 0)
        state->episodes_stored++;

    if (!cache_hit && g_have_cache && output[0])
        (void)cos_inference_cache_store(bsc, output, sigma_proof);

    {
        char subj[64];
        sscanf(goal, "%63s", subj);
        if (subj[0])
            (void)cos_world_update_belief(subj, "mentions", goal, sigma_proof);
    }

    cos_speculative_sigma_observe(domain_h, sigma_proof);

    if (g_cfg.enable_consciousness) {
        (void)cos_consciousness_measure(&g_conscious, sigma_proof, schans,
                                        meta, g_prev_conscious_sigma);
        g_prev_conscious_sigma = sigma_proof;
        (void)cos_awareness_log(&g_conscious);
        state->consciousness_level =
            cos_consciousness_classify_level(&g_conscious);
    }

    {
        float esr;
        int   tf, ts;

        cos_spike_engine_stats(&g_spike, &esr, &tf, &ts);
        (void)esr;
        (void)tf;
        (void)ts;
    }

    if (cos_cw_check(&g_ledger)) {
        cos_mission_pause(&g_mission, "coherence watchdog");
        state->pauses++;
    }

    state->sigma_mean = g_ledger.sigma_mean_session;
    state->sigma_trend = g_ledger.sigma_mean_delta;
    state->k_eff       = g_ledger.k_eff;
    if (state->sigma_mean_initial < 0.f) {
        state->sigma_mean_initial = state->sigma_mean;
        state->k_eff_initial      = state->k_eff;
    }

    state->elapsed_ms = omega_now_ms() - state->started_ms;

    if (g_ledger.coherence_status == COS_OMEGA_COHERENCE_AT_RISK) {
        cos_mission_pause(&g_mission, "coherence AT_RISK");
        state->pauses++;
    }

    if (omega_write_status(state) != 0)
        omega_skip("omega_write_status");

    state->turn++;

    omega_consolidate_turn(state);

    if (g_cfg.human_review_interval > 0
        && state->turn % g_cfg.human_review_interval == 0) {
        cos_mission_pause(&g_mission, "scheduled human review");
        state->pauses++;
    }

    if (g_omega_events_fp) {
        struct cos_omega_turn_emit row;
        float e_turn = state->energy_total_joules - g_omega_prev_energy_j;
        float c_turn = state->co2_total_grams - g_omega_prev_co2_g;

        g_omega_prev_energy_j = state->energy_total_joules;
        g_omega_prev_co2_g    = state->co2_total_grams;

        row.t_ms          = omega_now_ms();
        row.sigma         = sigma_proof;
        row.attribution   = (int)ea.source;
        row.gate_action   = gate_decision;
        row.cache_hit     = cache_hit;
        row.energy_turn_j = e_turn;
        row.co2_turn_g    = c_turn;
        row.latency_ms    = omega_now_ms() - omega_turn_t0_ms;

        if (cos_omega_emit_event(g_omega_events_fp, state, state->turn, &row)
            == 0) {
            omega_run_alert_rules(g_omega_events_fp, state, sigma_proof,
                                  e_turn);
            fflush(g_omega_events_fp);
        }
    }

    return 0;
}

int cos_omega_step(struct cos_omega_state *state)
{
    if (!g_inited || !state)
        return -1;
    if (!state->running || state->halt_requested)
        return 1;

    if (cos_omega_signal_halt_requested) {
        cos_omega_halt(state, "SIGINT");
        return 1;
    }

    if (omega_halt_file_present()) {
        omega_consume_halt_file();
        cos_omega_halt(state, "halt file");
        return 1;
    }

    if (omega_step_inner(state) != 0)
        return -1;

    if (g_cfg.sigma_halt_threshold > 1e-6f
        && state->sigma_mean >= g_cfg.sigma_halt_threshold) {
        cos_omega_halt(state, "sigma_mean halt threshold");
        return 1;
    }
    if (g_cfg.k_eff_halt_threshold > 1e-6f
        && state->k_eff < g_cfg.k_eff_halt_threshold) {
        cos_omega_halt(state, "k_eff halt threshold");
        return 1;
    }

    return 0;
}

int cos_omega_run(struct cos_omega_state *state)
{
    int64_t       t_end_ms = 0;
    const char   *hours_env = getenv("COS_OMEGA_HOURS_CAP");

    if (!state)
        return -1;
    if (hours_env && hours_env[0]) {
        double h = atof(hours_env);
        if (h > 1e-6)
            t_end_ms = state->started_ms + (int64_t)(h * 3600000.0);
    }

    for (;;) {
        if (!state->running || state->halt_requested)
            break;
        if (g_cfg.max_turns > 0 && state->turn >= g_cfg.max_turns)
            break;
        if (t_end_ms > 0 && omega_now_ms() >= t_end_ms)
            break;

        if (cos_omega_signal_halt_requested) {
            cos_omega_halt(state, "SIGINT");
            break;
        }

        if (cos_omega_step(state) != 0)
            break;
    }
    return 0;
}

int cos_omega_finish_session(struct cos_omega_state *state)
{
    char   lp[768];
    char   rp[768];
    char   om[768];
    char  *txt;
    FILE  *fp;
    const char *h;
    int     n_consol;

    if (!state || !g_inited)
        return -1;

    h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";

    snprintf(om, sizeof om, "%s/.cos/omega", h);
    if (mkdir(om, 0700) != 0 && errno != EEXIST)
        (void)0;

    if (cos_state_ledger_default_path(lp, sizeof lp) == 0)
        (void)cos_state_ledger_persist(&g_ledger, lp);

    n_consol = g_cfg.consolidate_interval > 0 ? g_cfg.consolidate_interval : 50;
    (void)cos_engram_consolidate(n_consol);

    snprintf(rp, sizeof rp, "%s/.cos/omega/last_report.txt", h);
    txt = cos_omega_report(state);
    if (!txt) {
        if (g_omega_events_fp != NULL) {
            fclose(g_omega_events_fp);
            g_omega_events_fp = NULL;
        }
        return -2;
    }
    fp = fopen(rp, "w");
    if (fp) {
        fputs(txt, fp);
        fclose(fp);
    }
    fputs(txt, stdout);
    fflush(stdout);
    free(txt);

    if (g_omega_events_fp != NULL) {
        fclose(g_omega_events_fp);
        g_omega_events_fp = NULL;
    }
    return 0;
}

static void omega_fmt_duration(int64_t ms, char *buf, size_t cap)
{
    int64_t s = ms / 1000LL;
    int64_t h = s / 3600LL;
    int64_t m = (s % 3600LL) / 60LL;
    int64_t sec = s % 60LL;

    snprintf(buf, cap, "%lldh %lldm %llds", (long long)h, (long long)m,
             (long long)sec);
}

char *cos_omega_report(const struct cos_omega_state *state)
{
    char                      dur[64];
    char                      trend[128];
    char                      smi[16];
    char                      kei[16];
    struct cos_green_score    gs;
    const struct cos_energy_measurement *life;
    size_t                    cap = 8192;
    char                     *buf = (char *)malloc(cap);
    int                       n;

    if (!buf || !state)
        return NULL;

    omega_fmt_duration(state->elapsed_ms, dur, sizeof dur);

    if (state->sigma_mean_initial < -0.5f)
        snprintf(smi, sizeof smi, "n/a");
    else
        snprintf(smi, sizeof smi, "%.4f", (double)state->sigma_mean_initial);

    if (state->k_eff_initial < -0.5f)
        snprintf(kei, sizeof kei, "n/a");
    else
        snprintf(kei, sizeof kei, "%.4f", (double)state->k_eff_initial);

    if (state->sigma_trend < -1e-6f)
        snprintf(trend, sizeof trend, "DECLINING (σ̄ falling — uncertainty down)");
    else if (state->sigma_trend > 1e-6f)
        snprintf(trend, sizeof trend, "RISING (σ̄ increasing)");
    else
        snprintf(trend, sizeof trend, "STABLE");

    life = cos_energy_lifetime_total();
    gs   = cos_green_calculate(life);

    n = snprintf(buf, cap,
                 "\n┌─────────────────────────────────────────┐\n"
                 "│ Ω-Loop Report                           │\n"
                 "│                                         │\n"
                 "│ Duration:    %-26s │\n"
                 "│ Turns:       %-26d │\n"
                 "│ σ_mean:      %.4f (vs start %s)           │\n"
                 "│ k_eff:       %.4f (vs start %s)            │\n"
                 "│ Consciousness: Level %d                  │\n"
                 "│                                         │\n"
                 "│ Skills learned:     %-19d │\n"
                 "│ Episodes stored:    %-19d │\n"
                 "│ Cache hits:         %-19d │\n"
                 "│ Rollbacks:          %-19d │\n"
                 "│ Pauses:             %-19d │\n"
                 "│                                         │\n"
                 "│ Energy:     %.4f J | %.6f gCO2           │\n"
                 "│ Grade:      %c (%.2f/100)                 │\n"
                 "│                                         │\n"
                 "│ σ trend: %s\n"
                 "│ Verdict: Ω integrates live modules (halt=%d) │\n"
                 "└─────────────────────────────────────────┘\n",
                 dur, state->turn, (double)state->sigma_mean, smi,
                 (double)state->k_eff, kei, state->consciousness_level,
                 state->skills_learned, state->episodes_stored, state->cache_hits,
                 state->rollbacks, state->pauses,
                 (double)state->energy_total_joules,
                 (double)state->co2_total_grams, gs.grade,
                 (double)gs.green_score, trend, state->halt_requested);
    if (n <= 0 || (size_t)n >= cap) {
        free(buf);
        return NULL;
    }
    return buf;
}

void cos_omega_print_status(const struct cos_omega_state *state)
{
    char *r;

    if (!state)
        return;
    r = cos_omega_report(state);
    if (r) {
        fputs(r, stdout);
        free(r);
    }
}

int cos_omega_emit_event(FILE                          *fp,
                           const struct cos_omega_state *state,
                           int                             turn_completed,
                           const struct cos_omega_turn_emit *row)
{
    const struct cos_energy_measurement *life;
    struct cos_green_score               gs;

    if (!fp || !state || !row)
        return -1;
    life = cos_energy_lifetime_total();
    gs   = cos_green_calculate(life);

    fprintf(fp,
            "{\"t\":%lld,\"turn\":%d,\"sigma\":%.6f,\"k_eff\":%.6f,"
            "\"attribution\":%d,\"action\":\"%s\",\"cache_hit\":%s,"
            "\"energy_j\":%.9f,\"co2_g\":%.9f,\"latency_ms\":%lld,"
            "\"consciousness\":%d,\"skills\":%d,\"episodes\":%d,"
            "\"sigma_trend\":%.6f,\"green_score\":%.4f,"
            "\"green_grade\":\"%c\"}\n",
            (long long)row->t_ms, turn_completed, (double)row->sigma,
            (double)state->k_eff, row->attribution,
            omega_action_json(row->gate_action),
            row->cache_hit ? "true" : "false",
            (double)row->energy_turn_j, (double)row->co2_turn_g,
            (long long)row->latency_ms, state->consciousness_level,
            state->skills_learned, state->episodes_stored,
            (double)state->sigma_trend, (double)gs.green_score, gs.grade);
    return 0;
}

int cos_omega_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_omega_config cfg;
    struct cos_omega_state  st;
    char                   *rep;

    memset(&cfg, 0, sizeof cfg);
    cfg.max_turns         = 1;
    cfg.simulation_mode   = 1;
    cfg.enable_consciousness = 0;

    if (cos_omega_init(&cfg, &st) != 0)
        return 1;
    if (cos_omega_step(&st) != 0)
        return 2;
    if (cos_omega_halt(&st, "test") != 0)
        return 3;
    rep = cos_omega_report(&st);
    if (!rep || !strstr(rep, "Ω-Loop"))
        return 4;
    free(rep);
#endif
    return 0;
}
