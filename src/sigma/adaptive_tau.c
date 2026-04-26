/* Graded conformal CSV → runtime τ_accept (cached). */

#include "adaptive_tau.h"

#include "pattern_keywords.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static float g_cached_tau = -1.0f;
static char  g_cached_path[1024];
static int     g_graded_temp_inited = 0;
static float   g_graded_temp        = 1.0f;
static char    g_cached_temp_path[1024];

static char  g_pat_json_path[1024];
static char *g_pat_json_buf;
static size_t g_pat_json_len;
static int    g_pat_json_loaded;

void cos_adaptive_tau_invalidate_cache(void)
{
    g_cached_tau = -1.0f;
    g_cached_path[0] = '\0';
    g_graded_temp_inited = 0;
    g_graded_temp        = 1.0f;
    g_cached_temp_path[0] = '\0';
    free(g_pat_json_buf);
    g_pat_json_buf      = NULL;
    g_pat_json_len      = 0;
    g_pat_json_loaded   = 0;
    g_pat_json_path[0]  = '\0';
}

static int adaptive_patterns_default_path(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    if (!buf || cap < 32)
        return -1;
    if (h == NULL || h[0] == '\0')
        h = "/tmp";
    if (snprintf(buf, cap, "%s/.cos/patterns.json", h) >= (int)cap)
        return -1;
    return 0;
}

static int adaptive_patterns_load(void)
{
    FILE *f;
    long  sz;

    if (g_pat_json_loaded && g_pat_json_buf != NULL)
        return 0;
    g_pat_json_loaded = 1;
    if (adaptive_patterns_default_path(g_pat_json_path, sizeof g_pat_json_path)
        != 0)
        return -1;
    f = fopen(g_pat_json_path, "r");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 8 || sz > 655360L) {
        fclose(f);
        return -1;
    }
    rewind(f);
    g_pat_json_buf = (char *)malloc((size_t)sz + 1u);
    if (!g_pat_json_buf) {
        fclose(f);
        return -1;
    }
    if (fread(g_pat_json_buf, 1u, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(g_pat_json_buf);
        g_pat_json_buf = NULL;
        return -1;
    }
    fclose(f);
    g_pat_json_buf[sz]     = '\0';
    g_pat_json_len         = (size_t)sz;
    return 0;
}

static float adaptive_patterns_find_tau(const char *json, const char *dom)
{
    char   needle[96];
    size_t dlen;
    const char *p;
    const char *end;

    if (!json || !dom || !dom[0])
        return -1.f;
    dlen = strlen(dom);
    if (dlen + 20 >= sizeof needle)
        return -1.f;
    if (snprintf(needle, sizeof needle, "\"domain\":\"%s\"", dom)
        >= (int)sizeof needle)
        return -1.f;
    end = json + strlen(json);
    p   = json;
    while ((p = strstr(p, needle)) != NULL) {
        const char *q = strstr(p, "\"optimal_tau\":");
        if (q != NULL && (size_t)(q - p) < 800u) {
            float t = (float)strtod(q + 14, NULL);
            if (t > 0.02f && t < 0.98f && t == t)
                return t;
        }
        p += 4;
        if (p >= end)
            break;
    }
    return -1.f;
}

float cos_adaptive_tau_for_prompt_domain(const char *prompt_utf8, float base)
{
    const char *en = getenv("COS_ADAPTIVE_TAU_PATTERNS");
    char        dom[80];
    float       t;

    if (en == NULL || en[0] != '1' || en[1] != '\0')
        return base;
    if (adaptive_patterns_load() != 0 || g_pat_json_buf == NULL)
        return base;
    cos_pattern_keyword_domain(prompt_utf8, dom, sizeof dom);
    t = adaptive_patterns_find_tau(g_pat_json_buf, dom);
    if (t < 0.f)
        return base;
    return t;
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

int cos_adaptive_tau_default_temperature_path(char *buf, size_t cap)
{
    if (!buf || cap < 16)
        return -1;
    const char *e = getenv("COS_ADAPTIVE_TEMPERATURE_TXT");
    if (e != NULL && e[0] != '\0') {
        if (snprintf(buf, cap, "%s", e) >= (int)cap)
            return -1;
        return 0;
    }
    const char *root = getenv("CREATION_OS_ROOT");
    if (root != NULL && root[0] != '\0') {
        if (snprintf(buf, cap, "%s/benchmarks/graded/temperature.txt", root)
            >= (int)cap)
            return -1;
        return 0;
    }
    if (snprintf(buf, cap, "benchmarks/graded/temperature.txt") >= (int)cap)
        return -1;
    return 0;
}

float cos_adaptive_graded_temperature(void)
{
    char          tmp[1024];
    const char   *path = NULL;

    if (g_graded_temp_inited)
        return g_graded_temp;

    g_graded_temp_inited = 1;
    g_graded_temp       = 1.0f;

    if (cos_adaptive_tau_default_temperature_path(tmp, sizeof(tmp)) == 0)
        path = tmp;
    if (path == NULL || path[0] == '\0') {
        return g_graded_temp;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(g_cached_temp_path, sizeof(g_cached_temp_path), "%s", path);
        return g_graded_temp;
    }
    char line[128];
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        snprintf(g_cached_temp_path, sizeof(g_cached_temp_path), "%s", path);
        return g_graded_temp;
    }
    fclose(f);
    {
        float T = (float)strtod(line, NULL);
        if (T < 1e-4f || T > 1e4f || T != T)
            T = 1.0f;
        g_graded_temp = T;
    }
    snprintf(g_cached_temp_path, sizeof(g_cached_temp_path), "%s", path);
    return g_graded_temp;
}

static float tau_apply_temperature_blend(float tau)
{
    const char *b = getenv("COS_ADAPTIVE_TAU_TEMPERATURE_BLEND");
    if (b == NULL || b[0] != '1' || b[1] != '\0')
        return tau;
    float T = cos_adaptive_graded_temperature();
    if (T < 1e-6f)
        return tau;
    /* Post-hoc calibration T>1 often tightens logits; nudge τ modestly. */
    float m = 1.0f / sqrtf(T);
    if (m < 0.75f)
        m = 0.75f;
    if (m > 1.25f)
        m = 1.25f;
    tau *= m;
    if (tau < 0.02f)
        tau = 0.02f;
    if (tau > 0.98f)
        tau = 0.98f;
    return tau;
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
    g_cached_tau = tau_apply_temperature_blend(g_cached_tau);
    snprintf(g_cached_path, sizeof(g_cached_path), "%s", path);
    return g_cached_tau;
}

float cos_adaptive_tau_accept(void)
{
    return cos_adaptive_tau_accept_from_csv(NULL, 0.3f);
}

float cos_adaptive_tau_rethink_from_accept(float tau_accept)
{
    if (tau_accept < 0.f)
        tau_accept = 0.f;
    if (tau_accept > 1.f)
        tau_accept = 1.f;
    return tau_accept + (1.0f - tau_accept) * 0.5f;
}

float cos_adaptive_tau_rethink(void)
{
    return cos_adaptive_tau_rethink_from_accept(cos_adaptive_tau_accept());
}

/* --- Runtime τ persistence (JSON) ------------------------------------ */

int cos_adaptive_tau_state_path(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    if (!buf || cap < 48)
        return -1;
    if (h == NULL || h[0] == '\0')
        h = "/tmp";
    if (snprintf(buf, cap, "%s/.cos/tau_state.json", h) >= (int)cap)
        return -1;
    return 0;
}

int cos_adaptive_tau_state_load(cos_adaptive_tau_t *at)
{
    char path[512], line[4096];
    if (at == NULL || cos_adaptive_tau_state_path(path, sizeof path) != 0)
        return -1;
    memset(at, 0, sizeof *at);
    at->tau_accept          = 0.40f;
    at->tau_rethink         = 0.60f;
    at->accuracy_estimate   = 0.5f;
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return -1;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, "\"tau_accept\":", 13) == 0)
            at->tau_accept = (float)strtod(line + 13, NULL);
        else if (strncmp(line, "\"tau_rethink\":", 14) == 0)
            at->tau_rethink = (float)strtod(line + 14, NULL);
        else if (strncmp(line, "\"n_history\":", 12) == 0)
            at->n_history = atoi(line + 12);
        else if (strncmp(line, "\"accuracy_estimate\":", 21) == 0)
            at->accuracy_estimate = (float)strtod(line + 21, NULL);
    }
    fclose(f);
    if (at->tau_accept < 0.02f || at->tau_accept > 0.98f || at->tau_accept != at->tau_accept)
        at->tau_accept = 0.40f;
    if (at->tau_rethink <= at->tau_accept || at->tau_rethink > 0.99f)
        at->tau_rethink = cos_adaptive_tau_rethink_from_accept(at->tau_accept);
    return 0;
}

int cos_adaptive_tau_state_save(const cos_adaptive_tau_t *at)
{
    char path[512];
    if (at == NULL || cos_adaptive_tau_state_path(path, sizeof path) != 0)
        return -1;
    {
        char dir[512];
        snprintf(dir, sizeof dir, "%s", path);
        char *slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            (void)mkdir(dir, 0700);
        }
    }
    FILE *f = fopen(path, "w");
    if (f == NULL)
        return -1;
    fprintf(f,
            "{\n  \"tau_accept\": %.6f,\n  \"tau_rethink\": %.6f,\n"
            "  \"n_history\": %d,\n  \"accuracy_estimate\": %.6f\n}\n",
            (double)at->tau_accept, (double)at->tau_rethink, at->n_history,
            (double)at->accuracy_estimate);
    fclose(f);
    return 0;
}

float cos_conformal_tau(const float *sigmas, const int *correct, int n,
                        float target_accuracy)
{
    int   i, j;
    float best = 0.35f;
    if (sigmas == NULL || correct == NULL || n <= 0)
        return best;
    for (i = 0; i < n; ++i) {
        float t = sigmas[i];
        if (t < 0.f || t > 1.f || t != t)
            continue;
        int ok = 0, tot = 0;
        for (j = 0; j < n; ++j) {
            if (sigmas[j] <= t + 1e-6f) {
                if (correct[j] < 0)
                    continue; /* unknown label — exclude from conformal ratio */
                tot++;
                if (correct[j])
                    ok++;
            }
        }
        if (tot > 0 && (float)ok / (float)tot >= target_accuracy - 1e-4f
            && t > best)
            best = t;
    }
    if (best < 0.05f)
        best = 0.05f;
    if (best > 0.95f)
        best = 0.95f;
    return best;
}

float cos_adaptive_tau_update(cos_adaptive_tau_t *at, float sigma, int was_correct)
{
    int nuse;
    if (at == NULL)
        return 0.4f;
    if (was_correct >= 0)
        at->accuracy_estimate =
            0.92f * at->accuracy_estimate + 0.08f * (was_correct ? 1.f : 0.f);
    {
        int ch = 0;
        if (was_correct > 0)
            ch = 1;
        else if (was_correct == 0)
            ch = 0;
        else
            ch = -1; /* unknown — σ logged but not used as false in conformal */
        if (at->n_history < 256) {
            at->sigma_history[at->n_history]   = sigma;
            at->correct_history[at->n_history] = ch;
            at->n_history++;
        } else {
            memmove(at->sigma_history, at->sigma_history + 1,
                    255u * sizeof(at->sigma_history[0]));
            memmove(at->correct_history, at->correct_history + 1,
                    255u * sizeof(at->correct_history[0]));
            at->sigma_history[255]   = sigma;
            at->correct_history[255] = ch;
        }
    }
    nuse = at->n_history;
    if (nuse >= 4) {
        int labeled = 0;
        for (int k = 0; k < nuse; ++k) {
            if (at->correct_history[k] >= 0) {
                labeled = 1;
                break;
            }
        }
        float ct = at->tau_accept;
        if (labeled)
            ct = cos_conformal_tau(at->sigma_history, at->correct_history, nuse,
                                   0.88f);
        at->tau_accept = 0.85f * at->tau_accept + 0.15f * ct;
        if (at->tau_accept < 0.08f)
            at->tau_accept = 0.08f;
        if (at->tau_accept > 0.85f)
            at->tau_accept = 0.85f;
        at->tau_rethink = cos_adaptive_tau_rethink_from_accept(at->tau_accept);
    }
    return at->tau_accept;
}
