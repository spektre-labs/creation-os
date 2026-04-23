/*
 * Coherence watchdog — background ledger polling + consecutive alarm latch.
 */

#include "coherence_watchdog.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#endif

volatile int g_cos_cw_pause_request;

static struct cos_cw_config g_cfg;
#if defined(__unix__) || defined(__APPLE__)
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       g_thr;
#endif
static volatile int g_running;
static volatile int g_started;
static int          g_consec;

void cos_cw_default_config(struct cos_cw_config *out)
{
    if (!out)
        return;
    out->k_eff_min        = 0.5f;
    out->sigma_max        = 0.8f;
    out->check_interval_ms = 30000;
    out->alarm_count      = 3;
}

void cos_cw_install_config(const struct cos_cw_config *config)
{
    cos_cw_default_config(&g_cfg);
    if (config != NULL)
        g_cfg = *config;
}

#if defined(__unix__) || defined(__APPLE__)
static void *watchdog_main(void *ctx)
{
    (void)ctx;
    while (g_running) {
        struct cos_state_ledger L;
        cos_state_ledger_init(&L);
        char path[768];
        if (cos_state_ledger_default_path(path, sizeof(path)) == 0
            && cos_state_ledger_load(&L, path) == 0) {
            if (cos_cw_check(&L))
                g_cos_cw_pause_request = 1;
        }
        int ms = g_cfg.check_interval_ms;
        if (ms < 100)
            ms = 100;
        usleep((useconds_t)(ms * 1000));
    }
    return NULL;
}
#endif

int cos_cw_start(const struct cos_cw_config *config)
{
#if !defined(__unix__) && !defined(__APPLE__)
    (void)config;
    return -1;
#else
    if (g_started)
        return -1;
    cos_cw_install_config(config);
    pthread_mutex_lock(&g_mu);
    g_consec = 0;
    pthread_mutex_unlock(&g_mu);
    g_cos_cw_pause_request = 0;
    g_running              = 1;
    if (pthread_create(&g_thr, NULL, watchdog_main, NULL) != 0) {
        g_running = 0;
        return -1;
    }
    g_started = 1;
    return 0;
#endif
}

int cos_cw_check(const struct cos_state_ledger *ledger)
{
    if (!ledger)
        return -1;
#if defined(__unix__) || defined(__APPLE__)
    pthread_mutex_lock(&g_mu);
#endif
    int bad = (ledger->k_eff < g_cfg.k_eff_min)
           || (ledger->sigma_mean_session > g_cfg.sigma_max);
    if (bad)
        g_consec++;
    else
        g_consec = 0;
    int fire = (g_consec >= g_cfg.alarm_count) ? 1 : 0;
#if defined(__unix__) || defined(__APPLE__)
    pthread_mutex_unlock(&g_mu);
#endif
    return fire;
}

int cos_cw_stop(void)
{
#if !defined(__unix__) && !defined(__APPLE__)
    return -1;
#else
    if (!g_started)
        return -1;
    g_running = 0;
    (void)pthread_join(g_thr, NULL);
    g_started = 0;
    pthread_mutex_lock(&g_mu);
    g_consec = 0;
    pthread_mutex_unlock(&g_mu);
    return 0;
#endif
}

int cos_cw_active(void)
{
    return g_started ? 1 : 0;
}

int cos_cw_self_test(void)
{
    struct cos_cw_config c;
    cos_cw_default_config(&c);
    c.alarm_count       = 2;
    c.check_interval_ms = 3600000;
    cos_cw_install_config(&c);

    struct cos_state_ledger ok;
    cos_state_ledger_init(&ok);
    ok.k_eff              = 0.9f;
    ok.sigma_mean_session = 0.2f;
    if (cos_cw_check(&ok) != 0)
        return -1;

    struct cos_state_ledger bad;
    cos_state_ledger_init(&bad);
    bad.k_eff              = 0.1f;
    bad.sigma_mean_session = 0.9f;
    if (cos_cw_check(&bad) != 0)
        return -2;
    if (cos_cw_check(&bad) != 1)
        return -3;
    if (cos_cw_check(NULL) != -1)
        return -4;

    cos_cw_default_config(&c);
    cos_cw_install_config(&c);
    return 0;
}
