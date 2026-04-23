/*
 * Axis-aligned rigid twin — Euler integration + collision probes.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "physics_model.h"

#include <math.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static float clampf(float x, float lo, float hi)
{
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

static void aabb_min_max(const float c[3], const float ext[3], float mn[3],
                         float mx[3])
{
    float hx = ext[0] * 0.5f;
    float hy = ext[1] * 0.5f;
    float hz = ext[2] * 0.5f;
    mn[0]  = c[0] - hx;
    mn[1]  = c[1] - hy;
    mn[2]  = c[2] - hz;
    mx[0]  = c[0] + hx;
    mx[1]  = c[1] + hy;
    mx[2]  = c[2] + hz;
}

static int aabb_overlap_centers(const float *ca, const float *ea,
                                const float *cb, const float *eb)
{
    float amin[3], amax[3], bmin[3], bmax[3];
    aabb_min_max(ca, ea, amin, amax);
    aabb_min_max(cb, eb, bmin, bmax);
    return amin[0] <= bmax[0] && amax[0] >= bmin[0] && amin[1] <= bmax[1]
           && amax[1] >= bmin[1] && amin[2] <= bmax[2] && amax[2] >= bmin[2];
}

int cos_physics_init(struct cos_physics_world *w)
{
    if (w == NULL)
        return -1;
    memset(w, 0, sizeof(*w));
    w->gravity   = 9.81f;
    w->friction  = 0.5f;
    w->agent_extent[0] = 0.45f;
    w->agent_extent[1] = 0.45f;
    w->agent_extent[2] = 1.75f;
    /* Identity quaternion + origin. */
    w->agent.orientation[0] = 0.f;
    w->agent.orientation[1] = 0.f;
    w->agent.orientation[2] = 0.f;
    w->agent.orientation[3] = 1.f;
    w->agent.battery          = 1.f;
    w->agent.temperature      = 25.f;
    w->agent.sensors_active   = 1;
    w->agent.actuators_active = 1;
    return 0;
}

int cos_physics_add_object(struct cos_physics_world *w,
                           const struct cos_physics_object *obj)
{
    if (w == NULL || obj == NULL || w->n_objects >= 128)
        return -1;
    w->objects[w->n_objects++] = *obj;
    return 0;
}

int cos_physics_simulate_step(struct cos_physics_world *w, float dt)
{
    int i;

    if (w == NULL || dt <= 0.f)
        return -1;

    for (i = 0; i < w->n_objects; ++i) {
        struct cos_physics_object *o = &w->objects[i];
        if (o->static_object)
            continue;
        if (o->mass <= 1e-6f)
            continue;
        /* Euler: approximate vertical drop with ground clamp at z = size_z/2. */
        o->position[2] -= 0.5f * w->gravity * dt * dt;
        if (o->position[2] < o->size[2] * 0.5f)
            o->position[2] = o->size[2] * 0.5f;
    }
    return 0;
}

int cos_physics_collision_check(const struct cos_physics_world *w,
                                const float position[3],
                                const float size[3])
{
    int i;

    if (w == NULL || position == NULL || size == NULL)
        return -1;

    for (i = 0; i < w->n_objects; ++i) {
        const struct cos_physics_object *o = &w->objects[i];
        if (!o->static_object)
            continue;
        if (aabb_overlap_centers(position, size, o->position, o->size))
            return 1;
    }
    return 0;
}

int cos_physics_predict_action(struct cos_physics_world *w,
                               const struct cos_action *action,
                               struct cos_physical_state *result)
{
    float dx, dy, dz;
    float speed;
    int   hit;

    if (w == NULL || action == NULL || result == NULL)
        return -1;

    *result = w->agent;

    switch (action->type) {
    case COS_ACTION_MOVE:
        dx = action->params[0];
        dy = action->params[1];
        dz = action->params[2];
        speed = sqrtf(dx * dx + dy * dy + dz * dz);
        result->position[0] += dx;
        result->position[1] += dy;
        result->position[2] += dz;
        result->velocity[0] = dx * 2.f;
        result->velocity[1] = dy * 2.f;
        result->velocity[2] = dz * 2.f;
        (void)speed;
        break;
    case COS_ACTION_LOOK: {
        float yaw   = clampf(action->params[0], -3.14159f, 3.14159f);
        float pitch = clampf(action->params[1], -1.2f, 1.2f);
        float cy    = cosf(yaw * 0.5f);
        float sy    = sinf(yaw * 0.5f);
        float cp    = cosf(pitch * 0.5f);
        float sp    = sinf(pitch * 0.5f);
        result->orientation[0] = sy * cp;
        result->orientation[1] = sp;
        result->orientation[2] = cy * sp;
        result->orientation[3] = cy * cp;
    } break;
    case COS_ACTION_WAIT:
    case COS_ACTION_GRASP:
    case COS_ACTION_RELEASE:
    case COS_ACTION_SPEAK:
    default:
        break;
    }

    hit = cos_physics_collision_check(w, result->position, w->agent_extent);
    return hit ? -2 : 0;
}

float cos_physics_prediction_sigma(int collision_flag, float motion_speed)
{
    float sp = motion_speed;
    if (sp < 0.f)
        sp = 0.f;
    if (collision_flag)
        return 0.92f;
    return clampf(0.08f + sp * 0.15f, 0.f, 0.85f);
}

#if CREATION_OS_ENABLE_SELF_TESTS
#include <stdio.h>
static int fail(const char *m)
{
    fprintf(stderr, "physics_model self-test: %s\n", m);
    return 1;
}
#endif

int cos_physics_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_physics_world w;
    struct cos_physics_object floor_obj;
    struct cos_action        act;
    struct cos_physical_state pred;
    int                       rc;

    if (cos_physics_init(&w) != 0)
        return fail("init");

    memset(&floor_obj, 0, sizeof floor_obj);
    floor_obj.id           = 1;
    floor_obj.static_object = 1;
    floor_obj.position[0] = 0.f;
    floor_obj.position[1] = 0.f;
    floor_obj.position[2] = -10.f;
    floor_obj.size[0]      = 100.f;
    floor_obj.size[1]      = 100.f;
    floor_obj.size[2]      = 4.f;

    if (cos_physics_add_object(&w, &floor_obj) != 0)
        return fail("add_floor");

    if (cos_physics_collision_check(&w, w.agent.position, w.agent_extent) != 0)
        return fail("spawn_overlap");

    memset(&act, 0, sizeof act);
    act.type             = COS_ACTION_MOVE;
    act.params[0]        = 0.f;
    act.params[1]        = 0.f;
    act.params[2]        = -10.f;
    rc = cos_physics_predict_action(&w, &act, &pred);
    if (rc != -2)
        return fail("predict_should_collide_floor");

    act.params[2] = 0.f;
    rc              = cos_physics_predict_action(&w, &act, &pred);
    if (rc != 0)
        return fail("predict_clear");

    if (cos_physics_simulate_step(&w, 0.016f) != 0)
        return fail("step");

    fprintf(stderr, "physics_model self-test: OK\n");
#endif
    return 0;
}
