/* Graded conformal CSV → runtime τ_accept (cached). */

#include "adaptive_tau.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float g_cached_tau = -1.0f;
static char  g_cached_path[1024];

void cos_adaptive_tau_invalidate_cache(void)
{
    g_cached_tau = -1.0f;
    g_cached_path[0] = '\0';
}

int cos_adaptive_tau_default_csv_path(char *buf, size_t cap)
{
    if (!buf || cap < 16)
        return -1;
    const char *e = getenv("COS_ADAPTIVE_TAU_CSV");
    if (e != NULL && e[0] != '\0') {
        if (snprintf(buf, cap, "%s", e) >= (int)cap)
            return -1;
        return 0;
    }
    const char *root = getenv("CREATION_OS_ROOT");
    if (root != NULL && root[0] != '\0') {
        if (snprintf(buf, cap, "%s/benchmarks/graded/conformal_thresholds.csv",
                     root) >= (int)cap)
            return -1;
        return 0;
    }
    if (snprintf(buf, cap, "benchmarks/graded/conformal_thresholds.csv")
        >= (int)cap)
        return -1;
    return 0;
}

float cos_adaptive_tau_accept_from_csv(const char *csv_path,
                                     float fallback_accept)
{
    const char *path = csv_path;
    char        tmp[1024];

    if (path == NULL || path[0] == '\0') {
        if (cos_adaptive_tau_default_csv_path(tmp, sizeof(tmp)) != 0)
            return fallback_accept;
        path = tmp;
    }

    if (g_cached_tau >= 0.f && strncmp(g_cached_path, path, sizeof(g_cached_path)) == 0)
        return g_cached_tau;

    FILE *f = fopen(path, "r");
    if (!f) {
        g_cached_tau = fallback_accept;
        snprintf(g_cached_path, sizeof(g_cached_path), "%s", path);
        return g_cached_tau;
    }

    char line[512];
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        g_cached_tau = fallback_accept;
        snprintf(g_cached_path, sizeof(g_cached_path), "%s", path);
        return g_cached_tau;
    }

    float best_tau = -1.f;
    float best_dist = 1e9f;

    while (fgets(line, sizeof(line), f) != NULL) {
        float alpha = 0.f, target = 0.f, tau = 0.f;
        float ta = 0.f, tcov = 0.f;
        int   test_ok = 0;
        int n = sscanf(line, "%f,%f,%f,%f,%f,%d",
                       &alpha, &target, &tau, &ta, &tcov, &test_ok);
        if (n < 3)
            continue;
        (void)alpha;
        if (target >= 0.89f && target <= 0.91f) {
            float d = (target > 0.9f) ? (target - 0.9f) : (0.9f - target);
            if (d < best_dist) {
                best_dist = d;
                best_tau  = tau;
            }
        }
    }
    fclose(f);

    if (best_tau < 0.f || best_tau > 1.f)
        g_cached_tau = fallback_accept;
    else
        g_cached_tau = best_tau;
    snprintf(g_cached_path, sizeof(g_cached_path), "%s", path);
    return g_cached_tau;
}

float cos_adaptive_tau_rethink_from_accept(float tau_accept)
{
    if (tau_accept < 0.f)
        tau_accept = 0.f;
    if (tau_accept > 1.f)
        tau_accept = 1.f;
    return tau_accept + (1.0f - tau_accept) * 0.5f;
}
