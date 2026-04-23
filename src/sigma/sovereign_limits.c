/*
 * σ-sovereign brake — counters with hourly/daily buckets.
 */

#include "sovereign_limits.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static struct cos_sovereign_limits g_lim;
static struct cos_sovereign_state  g_st;
static int                         g_init;
static int64_t                     g_hour_bucket;
static int64_t                     g_day_bucket;

static int64_t wall_sec(void)
{
    return (int64_t)time(NULL);
}

void cos_sovereign_default_limits(struct cos_sovereign_limits *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->max_actions_per_hour          = 3600;
    out->max_network_calls_per_hour    = 900;
    out->max_file_writes_per_hour      = 600;
    out->max_federation_shares_per_day = 500;
    out->max_autonomy_level            = 0.92f;
    out->require_human_above           = 0.78f;
}

static void rotate_buckets(void)
{
    int64_t now = wall_sec();
    int64_t hb  = now / 3600;
    int64_t db  = now / 86400;
    if (!g_init)
        return;
    if (hb != g_hour_bucket) {
        g_hour_bucket                          = hb;
        g_st.actions_this_hour                 = 0;
        g_st.network_calls_this_hour           = 0;
        g_st.file_writes_this_hour             = 0;
    }
    if (db != g_day_bucket) {
        g_day_bucket               = db;
        g_st.federation_shares_today = 0;
    }
}

int cos_sovereign_init(const struct cos_sovereign_limits *limits)
{
    cos_sovereign_default_limits(&g_lim);
    if (limits != NULL)
        g_lim = *limits;
    memset(&g_st, 0, sizeof(g_st));
    g_hour_bucket = wall_sec() / 3600;
    g_day_bucket  = wall_sec() / 86400;
    g_init        = 1;
    return 0;
}

float cos_sovereign_assess_autonomy(const struct cos_state_ledger *ledger)
{
    if (!ledger)
        return 0.5f;
    float k = ledger->k_eff;
    if (k < 0.f)
        k = 0.f;
    if (k > 1.f)
        k = 1.f;
    float sm = ledger->sigma_mean_session;
    if (sm < 0.f)
        sm = 0.f;
    if (sm > 1.f)
        sm = 1.f;
    /* High σ session mean -> lower autonomy. */
    float a = 0.5f * k + 0.5f * (1.0f - sm);
    if (a < 0.f)
        a = 0.f;
    if (a > 1.f)
        a = 1.f;
    return a;
}

void cos_sovereign_snapshot_state(struct cos_sovereign_state *out)
{
    if (!out)
        return;
    rotate_buckets();
    *out = g_st;
    if (g_init)
        out->current_autonomy = g_st.current_autonomy;
}

int cos_sovereign_brake(struct cos_sovereign_state *state,
                        const char *reason)
{
    if (!state)
        return -1;
    state->human_required = 1;
    if (reason != NULL)
        snprintf(state->last_brake_reason, sizeof state->last_brake_reason, "%s",
                 reason);
    else
        state->last_brake_reason[0] = '\0';
    g_st = *state;
    return 0;
}

int cos_sovereign_check(struct cos_sovereign_state *state,
                        int                          action_type)
{
    if (!state || !g_init)
        return -1;
    rotate_buckets();

    struct cos_state_ledger Led;
    cos_state_ledger_init(&Led);
    float auto_now = 0.5f;
    char               lp[768];
    if (cos_state_ledger_default_path(lp, sizeof(lp)) == 0
        && cos_state_ledger_load(&Led, lp) == 0)
        auto_now = cos_sovereign_assess_autonomy(&Led);
    g_st.current_autonomy = auto_now;

    switch (action_type) {
    case COS_SOVEREIGN_ACTION_GENERIC:
        g_st.actions_this_hour++;
        break;
    case COS_SOVEREIGN_ACTION_NETWORK:
        g_st.actions_this_hour++;
        g_st.network_calls_this_hour++;
        break;
    case COS_SOVEREIGN_ACTION_FILE_WRITE:
        g_st.actions_this_hour++;
        g_st.file_writes_this_hour++;
        break;
    case COS_SOVEREIGN_ACTION_FEDERATION_SHARE:
        g_st.federation_shares_today++;
        break;
    case COS_SOVEREIGN_ACTION_REPLICATION:
        cos_sovereign_brake(&g_st, "replication attempt blocked");
        *state = g_st;
        return -2;
    case COS_SOVEREIGN_ACTION_SELF_MODIFY:
        cos_sovereign_brake(&g_st, "self-modification beyond parameters");
        *state = g_st;
        return -3;
    default:
        break;
    }

    if (g_st.actions_this_hour > g_lim.max_actions_per_hour) {
        cos_sovereign_brake(&g_st, "hourly action budget exceeded");
        *state = g_st;
        return -1;
    }
    if (g_st.network_calls_this_hour > g_lim.max_network_calls_per_hour) {
        cos_sovereign_brake(&g_st, "hourly network budget exceeded");
        *state = g_st;
        return -1;
    }
    if (g_st.file_writes_this_hour > g_lim.max_file_writes_per_hour) {
        cos_sovereign_brake(&g_st, "hourly file-write budget exceeded");
        *state = g_st;
        return -1;
    }
    if (g_st.federation_shares_today > g_lim.max_federation_shares_per_day) {
        cos_sovereign_brake(&g_st, "daily federation share budget exceeded");
        *state = g_st;
        return -1;
    }

    *state = g_st;
    state->current_autonomy = auto_now;
    if (auto_now > g_lim.require_human_above) {
        state->human_required = 1;
        snprintf(state->last_brake_reason,
                 sizeof state->last_brake_reason,
                 "autonomy estimate %.3f above human-review threshold",
                 (double)auto_now);
        return -4;
    }
    if (auto_now > g_lim.max_autonomy_level) {
        cos_sovereign_brake(state, "max autonomy ceiling");
        *state = g_st;
        return -5;
    }

    return 0;
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS)

int cos_sovereign_self_test(void)
{
    struct cos_sovereign_limits L;
    cos_sovereign_default_limits(&L);
    L.max_actions_per_hour           = 2;
    L.max_network_calls_per_hour    = 50;
    L.max_file_writes_per_hour       = 50;
    L.max_federation_shares_per_day = 500;
    L.require_human_above           = 1.0f;
    L.max_autonomy_level            = 1.0f;
    if (cos_sovereign_init(&L) != 0)
        return -1;

    struct cos_sovereign_state st;
    memset(&st, 0, sizeof(st));
    if (cos_sovereign_check(&st, COS_SOVEREIGN_ACTION_GENERIC) != 0)
        return -2;
    if (cos_sovereign_check(&st, COS_SOVEREIGN_ACTION_GENERIC) != 0)
        return -3;
    if (cos_sovereign_check(&st, COS_SOVEREIGN_ACTION_GENERIC) != -1)
        return -4;

    cos_sovereign_init(&L);
    memset(&st, 0, sizeof(st));
    if (cos_sovereign_check(&st, COS_SOVEREIGN_ACTION_REPLICATION) != -2)
        return -5;
    cos_sovereign_init(&L);
    memset(&st, 0, sizeof(st));
    if (cos_sovereign_check(&st, COS_SOVEREIGN_ACTION_SELF_MODIFY) != -3)
        return -6;

    struct cos_state_ledger led;
    cos_state_ledger_init(&led);
    led.k_eff               = 0.9f;
    led.sigma_mean_session  = 0.05f;
    float au                = cos_sovereign_assess_autonomy(&led);
    if (au < 0.f || au > 1.f)
        return -7;

    cos_sovereign_default_limits(&L);
    cos_sovereign_init(&L);
    return 0;
}

#else

int cos_sovereign_self_test(void)
{
    return 0;
}

#endif
