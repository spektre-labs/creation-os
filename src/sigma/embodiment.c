/*
 * Embodiment orchestration — physics twin + sovereign gate + KG observations.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "embodiment.h"
#include "engram_episodic.h"
#include "error_attribution.h"
#include "knowledge_graph.h"
#include "sovereign_limits.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static int64_t wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000LL;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void push_history(struct cos_embodiment *e, const struct cos_action *a)
{
    int slot;

    if (e == NULL || a == NULL)
        return;
    slot             = e->n_history % 256;
    e->history[slot] = *a;
    e->n_history++;
}

static void embodiment_episode_store(struct cos_embodiment *e,
                                     const struct cos_action *a)
{
    struct cos_engram_episode ep;

    if (e == NULL || a == NULL)
        return;
    memset(&ep, 0, sizeof ep);
    ep.timestamp_ms = e->state.timestamp_ms;
    ep.prompt_hash  = cos_engram_prompt_hash(a->description);
    ep.sigma_combined =
        (a->sigma > e->last_sim_sigma) ? a->sigma : e->last_sim_sigma;
    ep.action = a->type;
    ep.was_correct =
        e->simulation_mode ? 1 : (e->last_sim_sigma < e->tau_physical ? 1 : 0);
    ep.attribution = COS_ERR_ALEATORIC;
    ep.ttt_applied = 0;
    (void)cos_engram_episode_store(&ep);
}

static void goal_lower(char *dst, size_t cap, const char *goal)
{
    size_t i;
    snprintf(dst, cap, "%s", goal != NULL ? goal : "");
    for (i = 0; i < cap && dst[i]; ++i)
        dst[i] = (char)tolower((unsigned char)dst[i]);
}

int cos_embodiment_init(struct cos_embodiment *e, int simulation_mode)
{
    if (e == NULL)
        return -1;
    memset(e, 0, sizeof(*e));
    e->simulation_mode = simulation_mode ? 1 : 0;
    e->tau_physical    = 0.42f;
    e->sigma_physical  = 0.12f;
    if (cos_physics_init(&e->physics) != 0)
        return -2;
    e->state.orientation[3] = 1.f;
    e->state.battery          = 1.f;
    e->state.temperature      = 22.f;
    e->state.sensors_active   = 1;
    e->state.actuators_active = 1;
    e->state.timestamp_ms     = wall_ms();
    memcpy(&e->physics.agent, &e->state, sizeof(e->state));
    return 0;
}

int cos_embodiment_perceive(struct cos_embodiment *e,
                            const struct cos_perception_result *perception)
{
    char buf[4608];

    if (e == NULL || perception == NULL)
        return -1;

    if (perception->sigma > e->sigma_physical)
        e->sigma_physical = perception->sigma;
    e->state.sensors_active = 1;
    e->state.timestamp_ms     = wall_ms();

    snprintf(buf, sizeof buf,
             "embodiment observation (sigma=%.4f): %s",
             perception->sigma, perception->description);
    if (cos_kg_init() == 0)
        (void)cos_kg_extract_and_store(buf);

    return 0;
}

int cos_embodiment_plan_action(struct cos_embodiment *e,
                               const char *goal,
                               struct cos_action *action)
{
    char gl[512];

    if (e == NULL || action == NULL)
        return -1;

    memset(action, 0, sizeof(*action));
    goal_lower(gl, sizeof gl, goal);

    if (strstr(gl, "forward") != NULL || strstr(gl, "move forward") != NULL) {
        action->type        = COS_ACTION_MOVE;
        action->params[0]   = 1.0f;
        action->params[1]   = 0.f;
        action->params[2]   = 0.f;
        action->sigma       = 0.2f;
        action->risk        = 0.15f;
        action->reversible  = 1;
        snprintf(action->description, sizeof action->description,
                 "MOVE forward 1m");
    } else if (strstr(gl, "back") != NULL) {
        action->type       = COS_ACTION_MOVE;
        action->params[0]  = -1.f;
        action->sigma      = 0.22f;
        action->risk       = 0.16f;
        action->reversible = 1;
        snprintf(action->description, sizeof action->description,
                 "MOVE back 1m");
    } else if (strstr(gl, "up") != NULL || strstr(gl, "lift") != NULL) {
        action->type       = COS_ACTION_MOVE;
        action->params[2]  = 0.25f;
        action->sigma      = 0.25f;
        action->risk       = 0.2f;
        action->reversible = 1;
        snprintf(action->description, sizeof action->description, "MOVE up");
    } else if (strstr(gl, "grasp") != NULL) {
        action->type       = COS_ACTION_GRASP;
        action->sigma      = 0.3f;
        action->risk       = 0.35f;
        action->reversible = 1;
        snprintf(action->description, sizeof action->description, "GRASP");
    } else if (strstr(gl, "release") != NULL || strstr(gl, "drop") != NULL) {
        action->type       = COS_ACTION_RELEASE;
        action->sigma      = 0.28f;
        action->risk       = 0.25f;
        action->reversible = 1;
        snprintf(action->description, sizeof action->description, "RELEASE");
    } else if (strstr(gl, "look") != NULL) {
        action->type       = COS_ACTION_LOOK;
        action->params[0]  = 0.2f;
        action->params[1]  = 0.05f;
        action->sigma      = 0.18f;
        action->risk       = 0.1f;
        action->reversible = 1;
        snprintf(action->description, sizeof action->description, "LOOK");
    } else if (strstr(gl, "wait") != NULL) {
        action->type       = COS_ACTION_WAIT;
        action->sigma      = 0.05f;
        action->risk       = 0.02f;
        action->reversible = 1;
        snprintf(action->description, sizeof action->description, "WAIT");
    } else {
        action->type       = COS_ACTION_WAIT;
        action->sigma      = 0.9f;
        action->risk       = 0.8f;
        action->reversible = 0;
        snprintf(action->description, sizeof action->description,
                 "UNPARSED_GOAL");
        return 1;
    }

    e->pending_action = *action;
    return 0;
}

int cos_embodiment_simulate(struct cos_embodiment *e,
                            const struct cos_action *action,
                            struct cos_physical_state *predicted)
{
    int   rc;
    int   col;
    float speed;

    if (e == NULL || action == NULL || predicted == NULL)
        return -1;

    memcpy(&e->physics.agent, &e->state, sizeof(e->state));
    rc = cos_physics_predict_action(&e->physics, action, predicted);
    col = (rc == -2);
    speed =
        sqrtf(action->params[0] * action->params[0]
              + action->params[1] * action->params[1]
              + action->params[2] * action->params[2]);

    e->last_sim_sigma = cos_physics_prediction_sigma(col, speed);
    e->last_predicted_state = *predicted;
    return rc;
}

int cos_embodiment_execute(struct cos_embodiment *e,
                           const struct cos_action *action)
{
    struct cos_physical_state pred;
    int                       sim_rc;
    struct cos_sovereign_state sv;

    if (e == NULL || action == NULL)
        return -1;

    sim_rc = cos_embodiment_simulate(e, action, &pred);
    (void)sim_rc;

    if (e->simulation_mode) {
        e->state = pred;
        e->state.timestamp_ms = wall_ms();
        push_history(e, action);
        embodiment_episode_store(e, action);
        return 0;
    }

    /* Real hardware path — four-way gate. */
    if (!action->reversible && e->last_sim_sigma > e->tau_physical * 0.85f)
        return -4;

    if (e->last_sim_sigma >= e->tau_physical)
        return -5;

    if (e->sigma_physical >= e->tau_physical)
        return -7;

    cos_sovereign_snapshot_state(&sv);
    if (sv.human_required)
        return -8;

    if (cos_sovereign_check(&sv, COS_SOVEREIGN_ACTION_GENERIC) != 0)
        return -6;

    e->state              = pred;
    e->state.timestamp_ms = wall_ms();
    push_history(e, action);
    embodiment_episode_store(e, action);
    return 0;
}

float cos_embodiment_sim_to_real_gap(
    const struct cos_physical_state *predicted,
    const struct cos_physical_state *actual)
{
    float d = 0.f;
    float dot;
    int   i;

    if (predicted == NULL || actual == NULL)
        return 1.f;

    for (i = 0; i < 3; ++i)
        d += fabsf(predicted->position[i] - actual->position[i]);

    dot = predicted->orientation[0] * actual->orientation[0]
          + predicted->orientation[1] * actual->orientation[1]
          + predicted->orientation[2] * actual->orientation[2]
          + predicted->orientation[3] * actual->orientation[3];
    if (dot < 0.f)
        dot = -dot;
    d *= (1.f / 3.f);
    d += (1.f - dot) * 0.35f;
    if (d < 0.f)
        d = 0.f;
    if (d > 1.f)
        d = 1.f;
    return d;
}

int cos_embodiment_update_world_model(struct cos_embodiment *e,
                                      float sim_to_real_gap)
{
    float g = sim_to_real_gap;
    if (e == NULL)
        return -1;
    if (g < 0.f)
        g = 0.f;
    if (g > 1.f)
        g = 1.f;
    e->sigma_sim_to_real =
        0.88f * e->sigma_sim_to_real + 0.12f * g;
    return 0;
}

void cos_embodiment_print_state(FILE *fp, const struct cos_embodiment *e)
{
    if (fp == NULL || e == NULL)
        return;
    fprintf(fp,
            "position: %.3f %.3f %.3f\n"
            "orientation (xyzw): %.3f %.3f %.3f %.3f\n"
            "velocity: %.3f %.3f %.3f\n"
            "battery: %.3f  temperature: %.2f C\n"
            "sigma_physical: %.3f  sigma_sim_to_real: %.3f  tau_physical: %.3f\n"
            "simulation_mode: %d\n",
            (double)e->state.position[0], (double)e->state.position[1],
            (double)e->state.position[2], (double)e->state.orientation[0],
            (double)e->state.orientation[1], (double)e->state.orientation[2],
            (double)e->state.orientation[3], (double)e->state.velocity[0],
            (double)e->state.velocity[1], (double)e->state.velocity[2],
            (double)e->state.battery, (double)e->state.temperature,
            (double)e->sigma_physical, (double)e->sigma_sim_to_real,
            (double)e->tau_physical, e->simulation_mode);
}

void cos_embodiment_print_history(FILE *fp, const struct cos_embodiment *e)
{
    int i;
    int n;

    if (fp == NULL || e == NULL)
        return;
    n = e->n_history;
    if (n > 256)
        n = 256;
    for (i = 0; i < n; ++i) {
        int idx;
        if (e->n_history <= 256)
            idx = e->n_history - 1 - i;
        else
            idx = (e->n_history - 1 - i + 256) % 256;
        const struct cos_action *a = &e->history[idx];
        fprintf(fp, "%2d  type=%d sigma=%.3f risk=%.3f rev=%d  %s\n", i,
                a->type, (double)a->sigma, (double)a->risk, a->reversible,
                a->description);
    }
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int fail(const char *m)
{
    fprintf(stderr, "embodiment self-test: %s\n", m);
    return 1;
}
#endif

int cos_embodiment_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_embodiment e;
    struct cos_action     act;
    struct cos_physical_state pred, actual;
    float                 gap;

    cos_sovereign_init(NULL);

    if (cos_embodiment_init(&e, 1) != 0)
        return fail("init");

    if (cos_embodiment_plan_action(&e, "move forward 1m", &act) != 0)
        return fail("plan");

    if (cos_embodiment_simulate(&e, &act, &pred) != 0)
        return fail("simulate");

    if (cos_embodiment_execute(&e, &act) != 0)
        return fail("execute_sim");

    actual      = pred;
    actual.position[0] += 0.05f;
    gap = cos_embodiment_sim_to_real_gap(&pred, &actual);
    if (gap < 0.f || gap > 1.f)
        return fail("gap_range");

    if (cos_embodiment_update_world_model(&e, gap) != 0)
        return fail("wm");

    fprintf(stderr, "embodiment self-test: OK\n");
#endif
    return 0;
}
