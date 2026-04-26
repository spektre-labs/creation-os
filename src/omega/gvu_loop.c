/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "gvu_loop.h"

#include "config_persist.h"
#include "evolver.h"
#include "prompt_bank.h"

#include "bitnet_server.h"
#include "engram_episodic.h"
#include "error_attribution.h"
#include "reinforce.h"
#include "semantic_sigma.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(CLOCK_REALTIME)
static int64_t gvu_wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}
#else
static int64_t gvu_wall_ms(void)
{
    return (int64_t)time(NULL) * 1000LL;
}
#endif

#define GVU_MAX_TURNS 256
#define GVU_KAPPA_MAX 1000

static FILE *gvu_jsonl_open(void)
{
    const char *h = getenv("HOME");
    char        dir[512], path[768];
    FILE       *fp;

    if (h == NULL || h[0] == '\0')
        h = "/tmp";
    if (snprintf(dir, sizeof dir, "%s/.cos/omega", h) >= (int)sizeof dir)
        return NULL;
#if defined(__unix__) || defined(__APPLE__)
    {
        char d2[512];
        if (snprintf(d2, sizeof d2, "%s/.cos", h) < (int)sizeof d2)
            (void)0;
        else
            mkdir(d2, 0700);
    }
    mkdir(dir, 0700);
#endif
    if (snprintf(path, sizeof path, "%s/gvu.jsonl", dir) >= (int)sizeof path)
        return NULL;
    fp = fopen(path, "a");
    return fp;
}

static void gvu_jsonl_ep(FILE *fp, int ep, float sigma_mean, float acc,
                           float kappa, const cos_evolver_config_t *cfg)
{
    if (!fp || !cfg)
        return;
    fprintf(fp,
            "{\"t\":%lld,\"gvu\":1,\"episode\":%d,\"sigma_mean\":%.6f,"
            "\"accuracy\":%.6f,\"kappa\":%.6f,"
            "\"tau_accept\":%.6f,\"tau_rethink\":%.6f}\n",
            (long long)gvu_wall_ms(), ep, (double)sigma_mean, (double)acc,
            (double)kappa, (double)cfg->tau_accept, (double)cfg->tau_rethink);
    fflush(fp);
}

static int gvu_resolve_port(int port)
{
    const char *pe;
    if (port > 0)
        return port;
    pe = getenv("COS_BITNET_SERVER_PORT");
    if (pe != NULL && pe[0] != '\0')
        return atoi(pe);
    return 11434;
}

int cos_gvu_run(int n_episodes, int episode_len, int port, int verbose,
                int simulate)
{
    cos_prompt_bank_t           pb;
    cos_omega_runtime_config_t  rtc;
    float                       kappa_hist[GVU_KAPPA_MAX];
    int                         kappa_n = 0;
    float                       prev_acc = -1.f;
    int                         ep, t, rc = 0;
    FILE                       *jfp = NULL;
    int                         use_port = gvu_resolve_port(port);

    if (n_episodes < 1 || n_episodes > 100000)
        return -1;
    if (episode_len < 1)
        return -1;
    if (episode_len > GVU_MAX_TURNS)
        episode_len = GVU_MAX_TURNS;

    memset(&pb, 0, sizeof pb);
    if (cos_prompt_bank_load(&pb) != 0) {
        fprintf(stderr,
                "[gvu] failed to load graded prompt bank "
                "(CREATION_OS_ROOT or cwd benchmarks/graded/graded_prompts.csv)\n");
        return -2;
    }

    cos_omega_runtime_config_default(&rtc);
    (void)cos_omega_runtime_config_load(&rtc);

    if (!simulate) {
        if (!cos_bitnet_server_is_healthy()
            && cos_bitnet_server_ensure() != 0) {
            fprintf(stderr,
                    "[gvu] bitnet server unavailable — use --sim or start "
                    "Ollama (COS_BITNET_SERVER_EXTERNAL=1)\n");
            return -3;
        }
    }

    jfp = gvu_jsonl_open();

    for (ep = 0; ep < n_episodes; ep++) {
        float        sigbuf[GVU_MAX_TURNS];
        int          okbuf[GVU_MAX_TURNS];
        uint64_t     seen[GVU_MAX_TURNS];
        int          n_seen = 0;
        float        sigma_sum = 0.f;
        int          correct_count = 0;
        int          total = 0;
        cos_evolver_config_t evcfg = rtc.ev;

        cos_prompt_bank_episode_begin(&pb);

        for (t = 0; t < episode_len; t++) {
            int               idx;
            uint64_t          ph;
            cos_banked_prompt_t *row;
            char             *resp = NULL;
            float             sigma = 0.5f;
            int               grade;
            struct cos_engram_episode epg;
            cos_semantic_sigma_result ssr;
            int               act;

            idx = cos_prompt_bank_pick(&pb, ep, t, seen, n_seen, &ph);
            if (idx < 0 || idx >= pb.n)
                break;
            row = &pb.rows[idx];
            seen[n_seen++] = ph;

            if (simulate) {
                size_t L = strlen(row->answer) + 96u;
                resp     = (char *)malloc(L);
                if (!resp) {
                    rc = -5;
                    goto cleanup;
                }
                snprintf(resp, L, "Simulated: the answer is %s.", row->answer);
                sigma = 0.35f + 0.01f * (float)((ep + t) % 5);
            } else {
                const char *model = rtc.preferred_model;
                float        temp = evcfg.semantic_temp_high;
                if (temp <= 0.f || temp > 2.f)
                    temp = 0.8f;
                if (model == NULL || strcmp(model, "default") == 0)
                    model = getenv("COS_BITNET_CHAT_MODEL");
                resp = cos_bitnet_query_model(use_port, row->prompt, NULL,
                                              model, temp,
                                              evcfg.semantic_max_tokens > 0
                                                  ? evcfg.semantic_max_tokens
                                                  : 128);
                if (resp == NULL) {
                    fprintf(stderr, "[gvu] generation failed turn t=%d\n", t);
                    rc = -4;
                    goto cleanup;
                }
                memset(&ssr, 0, sizeof ssr);
                sigma = cos_semantic_sigma_ex(
                    row->prompt, NULL, use_port, model, 3, resp, &ssr);
                if (!(sigma == sigma) || sigma < 0.f)
                    sigma = 0.99f;
                if (sigma > 1.f)
                    sigma = 1.f;
            }

            grade = cos_prompt_bank_grade(row->answer, row->category, resp);
            act   = cos_sigma_reinforce_round(sigma, evcfg.tau_accept,
                                            evcfg.tau_rethink, 0);

            memset(&epg, 0, sizeof epg);
            epg.timestamp_ms   = gvu_wall_ms();
            epg.prompt_hash    = ph;
            epg.sigma_combined = sigma;
            epg.action         = act;
            epg.was_correct    = grade;
            epg.attribution    = COS_ERR_NONE;
            epg.ttt_applied    = 0;
            epg.turn_timeout   = 0;
            (void)cos_engram_episode_store(&epg);

            row->times_seen++;
            row->last_sigma = 0.9f * row->last_sigma + 0.1f * sigma;
            if (grade)
                row->times_correct++;

            sigbuf[total] = sigma;
            okbuf[total] = grade ? 1 : 0;
            sigma_sum += sigma;
            if (grade)
                correct_count++;
            total++;

            free(resp);
        }

        if (total <= 0)
            break;

        {
            float acc = (float)correct_count / (float)total;
            float sigma_mean = sigma_sum / (float)total;
            float kappa      = (prev_acc >= 0.f) ? (acc - prev_acc) : 0.f;
            prev_acc = acc;

            if (kappa_n < GVU_KAPPA_MAX)
                kappa_hist[kappa_n++] = kappa;

            evcfg = cos_evolver_step(evcfg, sigbuf, total, okbuf, total);
            rtc.ev = evcfg;
            rtc.last_episode++;
            rtc.last_kappa = kappa;
            (void)cos_omega_runtime_config_save(&rtc);

            {
                char tb[32];
                snprintf(tb, sizeof tb, "%.4f", (double)evcfg.tau_accept);
                (void)setenv("COS_THINK_TAU_ACCEPT", tb, 1);
                snprintf(tb, sizeof tb, "%.4f", (double)evcfg.tau_rethink);
                (void)setenv("COS_THINK_TAU_RETHINK", tb, 1);
            }

            if (verbose)
                fprintf(stderr,
                        "[GVU ep=%d] sigma_mean=%.3f acc=%.1f%% kappa=%+.3f "
                        "tau_a=%.3f tau_r=%.3f\n",
                        ep, (double)sigma_mean, (double)(acc * 100.0),
                        (double)kappa, (double)evcfg.tau_accept,
                        (double)evcfg.tau_rethink);

            if (jfp)
                gvu_jsonl_ep(jfp, ep, sigma_mean, acc, kappa, &evcfg);

            if (kappa_n >= 3) {
                float k0 = kappa_hist[kappa_n - 3];
                float k1 = kappa_hist[kappa_n - 2];
                float k2 = kappa_hist[kappa_n - 1];
                int edge = (kappa_n == 3);
                if (!edge && kappa_n >= 4)
                    edge = (kappa_hist[kappa_n - 4] <= 0.f);
                if (k0 > 0.f && k1 > 0.f && k2 > 0.f && edge) {
                    rtc.ignition_count++;
                    (void)cos_omega_runtime_config_save(&rtc);
                    fprintf(stderr,
                            "\n[IGNITION] kappa > 0 for three consecutive "
                            "episodes (capability gain sustained).\n\n");
                }
            }

            (void)cos_engram_consolidate(50);
        }
    }

cleanup:
    if (jfp)
        fclose(jfp);
    if (verbose && kappa_n > 0) {
        int j;
        fputs("\n=== GVU REPORT ===\n", stderr);
        fprintf(stderr, "Episodes: %d x %d turns (requested)\n", n_episodes,
                episode_len);
        fputs("kappa trajectory (per episode): ", stderr);
        for (j = 0; j < kappa_n; j++)
            fprintf(stderr, "%+.3f ", (double)kappa_hist[j]);
        fputc('\n', stderr);
    }
    return rc;
}

int cos_gvu_self_test(void)
{
    if (cos_gvu_run(2, 3, 0, 0, 1) != 0)
        return 1;
    return 0;
}
