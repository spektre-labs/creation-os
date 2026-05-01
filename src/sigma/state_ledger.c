/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-state ledger implementation. */
#include "state_ledger.h"

#include "reinforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#if defined(CLOCK_REALTIME)
static int64_t cos_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return (int64_t)time(NULL) * 1000;
    return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
}
#elif defined(__APPLE__)
static int64_t cos_now_ms(void) {
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    uint64_t nano = t * (uint64_t)tb.numer / (uint64_t)tb.denom;
    return (int64_t)(nano / 1000000ULL);
}
#else
static int64_t cos_now_ms(void) {
    return (int64_t)time(NULL) * 1000;
}
#endif

typedef struct {
    float prev_sigma_c;
    float prev_k;
    float prev_dk;
    int   n;
    int   omega_gen;
} roll_t;

static roll_t g_roll;

static void drift_ensure_env_baseline(struct cos_state_ledger *l)
{
    if (!l || l->drift_baseline_from_env)
        return;
    const char *bm = getenv("COS_SIGMA_DRIFT_BASELINE_MEAN");
    const char *bs = getenv("COS_SIGMA_DRIFT_BASELINE_STD");
    if (bm == NULL || bs == NULL || bm[0] == '\0' || bs[0] == '\0')
        return;
    l->drift_baseline_mean     = (float)strtod(bm, NULL);
    l->drift_baseline_std      = (float)strtod(bs, NULL);
    l->drift_baseline_from_env = 1;
    l->drift_baseline_ready    = 1;
    if (l->drift_baseline_std < 1e-5f)
        l->drift_baseline_std = 1e-5f;
}

static void drift_push(struct cos_state_ledger *l, float s)
{
    if (!l)
        return;
    if (s < 0.f)
        s = 0.f;
    if (s > 1.f)
        s = 1.f;

    drift_ensure_env_baseline(l);

    if (!l->drift_baseline_from_env && l->drift_n_first20 < 20) {
        l->drift_first20[l->drift_n_first20++] = s;
        if (l->drift_n_first20 == 20) {
            float sum = 0.f;
            for (int i = 0; i < 20; ++i)
                sum += l->drift_first20[i];
            l->drift_baseline_mean = sum * (1.f / 20.f);
            float v = 0.f;
            for (int i = 0; i < 20; ++i) {
                float d = l->drift_first20[i] - l->drift_baseline_mean;
                v += d * d;
            }
            l->drift_baseline_std = sqrtf(v * (1.f / 20.f));
            if (l->drift_baseline_std < 1e-5f)
                l->drift_baseline_std = 1e-5f;
            l->drift_baseline_ready = 1;
        }
    }

    if (l->drift_ring_fill < COS_LEDGER_DRIFT_CAP) {
        l->drift_ring[l->drift_ring_fill++] = s;
    } else {
        l->drift_ring[l->drift_ring_head] = s;
        l->drift_ring_head = (l->drift_ring_head + 1) % COS_LEDGER_DRIFT_CAP;
    }
}

static float drift_window_mean(const struct cos_state_ledger *l)
{
    if (!l || l->drift_ring_fill <= 0)
        return 0.f;
    int n = l->drift_ring_fill;
    if (n < COS_LEDGER_DRIFT_CAP) {
        float sum = 0.f;
        for (int i = 0; i < n; ++i)
            sum += l->drift_ring[i];
        return sum / (float)n;
    }
    float sum = 0.f;
    for (int i = 0; i < COS_LEDGER_DRIFT_CAP; ++i)
        sum += l->drift_ring[(l->drift_ring_head + i) % COS_LEDGER_DRIFT_CAP];
    return sum * (1.f / (float)COS_LEDGER_DRIFT_CAP);
}

static void drift_evaluate(struct cos_state_ledger *l)
{
    if (!l || !l->drift_baseline_ready)
        return;
    if (l->drift_baseline_std <= 1e-6f)
        return;
    if (l->drift_ring_fill < 20)
        return;

    float cur = drift_window_mean(l);
    float z   = (cur - l->drift_baseline_mean) / l->drift_baseline_std;
    if (z > 2.0f || z < -2.0f) {
        l->drift_detected = 1;
        if (!l->drift_stderr_latched) {
            fprintf(stderr,
                    "[DRIFT] σ_mean=%.3f baseline=%.3f z=%.1f → RECALIBRATE\n",
                    (double)cur, (double)l->drift_baseline_mean, (double)z);
            l->drift_stderr_latched = 1;
        }
    }
}

void cos_state_ledger_init(struct cos_state_ledger *l) {
    if (!l) return;
    memset(l, 0, sizeof(*l));
    memset(&g_roll, 0, sizeof(g_roll));
    l->timestamp_ms = cos_now_ms();
}

void cos_state_ledger_fill_multi(struct cos_state_ledger *l,
                                 const cos_multi_sigma_t *m) {
    if (!l || !m) return;
    l->sigma_logprob     = m->sigma_logprob;
    l->sigma_entropy     = m->sigma_entropy;
    l->sigma_perplexity  = m->sigma_perplexity;
    l->sigma_consistency = m->sigma_consistency;
    l->sigma_combined    = m->sigma_combined;
}

void cos_state_ledger_note_cache_hit(struct cos_state_ledger *l) {
    if (!l) return;
    l->cache_hits += 1;
}

void cos_state_ledger_note_spec_decode(struct cos_state_ledger *l,
                                         int used_draft) {
    if (!l) return;
    if (used_draft)
        l->spec_decode_drafts += 1;
    else
        l->spec_decode_verifies += 1;
}

void cos_state_ledger_add_cost(struct cos_state_ledger *l, float eur) {
    if (!l) return;
    if (eur > 0.f) l->cost_total_eur += eur;
}

void cos_state_ledger_update(struct cos_state_ledger *l,
                             float sigma_combined,
                             float *meta,
                             int action) {
    if (!l) return;

    if (meta) {
        l->meta_perception    = meta[0];
        l->meta_self          = meta[1];
        l->meta_social        = meta[2];
        l->meta_situational   = meta[3];
    }

    l->sigma_combined = sigma_combined;
    l->k_eff = 1.0f - sigma_combined;
    if (l->k_eff < 0.f) l->k_eff = 0.f;
    if (l->k_eff > 1.f) l->k_eff = 1.f;

    /* Incremental session mean */
    g_roll.n += 1;
    float prev_mean = l->sigma_mean_session;
    l->sigma_mean_session =
        prev_mean + (sigma_combined - prev_mean) / (float)g_roll.n;

    l->sigma_mean_delta =
        sigma_combined - g_roll.prev_sigma_c;
    g_roll.prev_sigma_c = sigma_combined;

    float dk = l->k_eff - g_roll.prev_k;
    l->dk_dt = dk;
    l->d2k_dt2 = dk - g_roll.prev_dk;
    g_roll.prev_dk = dk;
    g_roll.prev_k = l->k_eff;

    /* Coherence bucket (mirrors cos chat verbose heuristic). */
    float mean_s = l->sigma_mean_session;
    if (mean_s < 0.5f && fabsf(l->dk_dt) < 0.05f) {
        l->coherence_status = 0;
    } else if (mean_s < 0.7f) {
        l->coherence_status = 1;
    } else {
        l->coherence_status = 2;
    }

    l->total_queries += 1;
    if (action == COS_SIGMA_ACTION_ACCEPT) {
        l->accepts += 1;
    } else if (action == COS_SIGMA_ACTION_RETHINK) {
        l->rethinks += 1;
    } else if (action == COS_SIGMA_ACTION_ABSTAIN) {
        l->abstains += 1;
    }

    g_roll.omega_gen += 1;
    l->omega_generation = g_roll.omega_gen;
    l->pending_actions =
        (l->rethinks > 0) ? 1 : 0;
    l->max_risk_level = (l->coherence_status > 1) ? 2 : l->coherence_status;

    drift_push(l, sigma_combined);
    drift_evaluate(l);

    l->timestamp_ms = cos_now_ms();
}

int cos_state_ledger_default_path(char *buf, size_t cap) {
    if (!buf || cap < 8) return -1;
    const char *home = getenv("HOME");
    if (!home || !home[0]) return -1;
    snprintf(buf, cap, "%s/.cos/state_ledger.json", home);
    return 0;
}

char *cos_state_ledger_to_json(const struct cos_state_ledger *l) {
    if (!l) return NULL;
    char *out = (char *)malloc(3000);
    if (!out) return NULL;
    snprintf(out, 3000,
        "{"
        "\"sigma_logprob\":%.6f,\"sigma_entropy\":%.6f,"
        "\"sigma_perplexity\":%.6f,\"sigma_consistency\":%.6f,"
        "\"sigma_combined\":%.6f,"
        "\"meta_perception\":%.6f,\"meta_self\":%.6f,"
        "\"meta_social\":%.6f,\"meta_situational\":%.6f,"
        "\"k_eff\":%.6f,\"dk_dt\":%.6f,\"d2k_dt2\":%.6f,"
        "\"coherence_status\":%d,"
        "\"total_queries\":%d,\"accepts\":%d,\"rethinks\":%d,"
        "\"abstains\":%d,\"cache_hits\":%d,"
        "\"spec_decode_drafts\":%d,\"spec_decode_verifies\":%d,"
        "\"cost_total_eur\":%.6f,"
        "\"sigma_mean_session\":%.6f,\"sigma_mean_delta\":%.6f,"
        "\"omega_generation\":%d,"
        "\"pending_actions\":%d,\"max_risk_level\":%d,"
        "\"sigma_drift\":%d,"
        "\"drift_baseline_mean\":%.6f,\"drift_baseline_std\":%.6f,"
        "\"timestamp_ms\":%lld"
        "}",
        (double)l->sigma_logprob, (double)l->sigma_entropy,
        (double)l->sigma_perplexity, (double)l->sigma_consistency,
        (double)l->sigma_combined,
        (double)l->meta_perception, (double)l->meta_self,
        (double)l->meta_social, (double)l->meta_situational,
        (double)l->k_eff, (double)l->dk_dt, (double)l->d2k_dt2,
        l->coherence_status,
        l->total_queries, l->accepts, l->rethinks,
        l->abstains, l->cache_hits,
        l->spec_decode_drafts, l->spec_decode_verifies,
        (double)l->cost_total_eur,
        (double)l->sigma_mean_session, (double)l->sigma_mean_delta,
        l->omega_generation,
        l->pending_actions, l->max_risk_level,
        (int)l->drift_detected,
        (double)l->drift_baseline_mean, (double)l->drift_baseline_std,
        (long long)l->timestamp_ms);
    return out;
}

static float grab_float(const char *json, const char *key, float def) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    return (float)strtod(p, NULL);
}

static int grab_int(const char *json, const char *key, int def) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    return (int)strtol(p, NULL, 10);
}

static int64_t grab_i64(const char *json, const char *key, int64_t def) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    return (int64_t)strtoll(p, NULL, 10);
}

int cos_state_ledger_load(struct cos_state_ledger *l,
                          const char *path) {
    if (!l || !path) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n <= 0 || n > 65536) {
        fclose(fp);
        return -1;
    }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        free(buf);
        fclose(fp);
        return -1;
    }
    buf[n] = '\0';
    fclose(fp);

    cos_state_ledger_init(l);
    l->sigma_logprob     = grab_float(buf, "sigma_logprob", 0.f);
    l->sigma_entropy     = grab_float(buf, "sigma_entropy", 0.f);
    l->sigma_perplexity  = grab_float(buf, "sigma_perplexity", 0.f);
    l->sigma_consistency = grab_float(buf, "sigma_consistency", 0.f);
    l->sigma_combined    = grab_float(buf, "sigma_combined", 0.f);
    l->meta_perception   = grab_float(buf, "meta_perception", 0.f);
    l->meta_self         = grab_float(buf, "meta_self", 0.f);
    l->meta_social       = grab_float(buf, "meta_social", 0.f);
    l->meta_situational  = grab_float(buf, "meta_situational", 0.f);
    l->k_eff             = grab_float(buf, "k_eff", 0.f);
    l->dk_dt             = grab_float(buf, "dk_dt", 0.f);
    l->d2k_dt2           = grab_float(buf, "d2k_dt2", 0.f);
    l->coherence_status  = grab_int(buf, "coherence_status", 0);
    l->total_queries     = grab_int(buf, "total_queries", 0);
    l->accepts           = grab_int(buf, "accepts", 0);
    l->rethinks          = grab_int(buf, "rethinks", 0);
    l->abstains          = grab_int(buf, "abstains", 0);
    l->cache_hits        = grab_int(buf, "cache_hits", 0);
    l->spec_decode_drafts   = grab_int(buf, "spec_decode_drafts", 0);
    l->spec_decode_verifies = grab_int(buf, "spec_decode_verifies", 0);
    l->cost_total_eur    = grab_float(buf, "cost_total_eur", 0.f);
    l->sigma_mean_session = grab_float(buf, "sigma_mean_session", 0.f);
    l->sigma_mean_delta   = grab_float(buf, "sigma_mean_delta", 0.f);
    l->omega_generation   = grab_int(buf, "omega_generation", 0);
    l->pending_actions    = grab_int(buf, "pending_actions", 0);
    l->max_risk_level     = grab_int(buf, "max_risk_level", 0);
    l->drift_detected     = (signed char)grab_int(buf, "sigma_drift", 0);
    l->drift_baseline_mean = grab_float(buf, "drift_baseline_mean", 0.f);
    l->drift_baseline_std  = grab_float(buf, "drift_baseline_std", 0.f);
    l->timestamp_ms       = grab_i64(buf, "timestamp_ms", 0);
    free(buf);
    return 0;
}

int cos_state_ledger_persist(const struct cos_state_ledger *l,
                             const char *path) {
    if (!l || !path) return -1;
    char *js = cos_state_ledger_to_json(l);
    if (!js) return -1;
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(js);
        return -1;
    }
    fputs(js, fp);
    fputc('\n', fp);
    fclose(fp);
    free(js);
    return 0;
}

void cos_state_ledger_print_summary(FILE *fp,
                                    const struct cos_state_ledger *l) {
    if (!fp || !l) return;
    static const char *coh[] = {"STABLE", "DRIFTING", "AT_RISK"};
    int c = l->coherence_status;
    if (c < 0 || c > 2) c = 0;
    fprintf(fp,
            "ledger: queries=%d accepts=%d rethinks=%d abstains=%d "
            "cache_hits=%d cost_€=%.4f\n"
            "        σ_mean=%.3f Δσ=%.3f k_eff=%.2f coherence=%s\n",
            l->total_queries, l->accepts, l->rethinks, l->abstains,
            l->cache_hits, (double)l->cost_total_eur,
            (double)l->sigma_mean_session, (double)l->sigma_mean_delta,
            (double)l->k_eff, coh[c]);
    if (l->spec_decode_drafts + l->spec_decode_verifies > 0) {
        int tot = l->spec_decode_drafts + l->spec_decode_verifies;
        float ratio =
            tot > 0 ? (float)l->spec_decode_drafts / (float)tot : 0.f;
        fprintf(fp,
                "        spec_decode: draft=%d verify=%d draft_ratio=%.2f\n",
                l->spec_decode_drafts, l->spec_decode_verifies,
                (double)ratio);
    }
    if (l->drift_detected) {
        fprintf(fp,
                "        σ_drift: RECALIBRATE recommended "
                "(baseline=%.3f std=%.3f)\n",
                (double)l->drift_baseline_mean,
                (double)l->drift_baseline_std);
    }
}

int cos_state_ledger_self_test(void) {
    struct cos_state_ledger L;
    cos_state_ledger_init(&L);
    cos_multi_sigma_t m = {
        .sigma_logprob = 0.1f,
        .sigma_entropy = 0.2f,
        .sigma_perplexity = 0.3f,
        .sigma_consistency = 0.4f,
        .sigma_combined = 0.5f,
    };
    cos_state_ledger_fill_multi(&L, &m);
    if (L.sigma_logprob > 0.11f || L.sigma_logprob < 0.09f) return 1;

    float meta[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    cos_state_ledger_update(&L, 0.5f, meta, COS_SIGMA_ACTION_ACCEPT);
    if (L.total_queries != 1 || L.accepts != 1) return 2;

    cos_state_ledger_update(&L, 0.6f, meta, COS_SIGMA_ACTION_RETHINK);
    if (L.rethinks != 1 || L.total_queries != 2) return 3;

    char *js = cos_state_ledger_to_json(&L);
    if (!js || strstr(js, "sigma_combined") == NULL) {
        free(js);
        return 4;
    }

    if (cos_state_ledger_persist(&L, "/tmp/cos_state_ledger_self_test.json") != 0) {
        free(js);
        return 5;
    }
    free(js);

    struct cos_state_ledger L2;
    if (cos_state_ledger_load(&L2, "/tmp/cos_state_ledger_self_test.json") != 0)
        return 6;
    if (L2.total_queries != L.total_queries) return 7;

    struct cos_state_ledger L3;
    cos_state_ledger_init(&L3);
    cos_state_ledger_note_cache_hit(&L3);
    if (L3.cache_hits != 1) return 8;

    return 0;
}
