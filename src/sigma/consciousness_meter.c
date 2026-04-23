/*
 * Operational consciousness index from σ-channel geometry (research metric).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "consciousness_meter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

static float clamp01(float x)
{
    if (x < 0.f)
        return 0.f;
    if (x > 1.f)
        return 1.f;
    return x;
}

static int64_t wall_ms(void)
{
#if defined(__APPLE__)
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    uint64_t nano = t * (uint64_t)tb.numer / (uint64_t)tb.denom;
    return (int64_t)(nano / 1000000ULL);
#elif defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000LL;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
#else
    return (int64_t)time(NULL) * 1000LL;
#endif
}

static float channel_integration(const float ch[4])
{
    float acc = 0.f;
    int   n   = 0;
    int   i, j;

    for (i = 0; i < 4; ++i) {
        for (j = i + 1; j < 4; ++j) {
            acc += 1.f - fabsf(ch[i] - ch[j]);
            n++;
        }
    }
    return n > 0 ? clamp01(acc / (float)n) : 0.f;
}

static float channel_differentiation(const float ch[4])
{
    float m = 0.f;
    float v;
    int   i;

    for (i = 0; i < 4; ++i)
        m += ch[i];
    m *= 0.25f;
    v = 0.f;
    for (i = 0; i < 4; ++i) {
        float d = ch[i] - m;
        v += d * d;
    }
    v *= 0.25f;
    return clamp01(v / 0.25f);
}

static float self_model_acc(const float ch[4], const float meta[4])
{
    float s = 0.f;
    int   i;

    for (i = 0; i < 4; ++i)
        s += 1.f - fabsf(meta[i] - ch[i]);
    return clamp01(s * 0.25f);
}

static float meta_aware_score(const float ch[4], const float meta[4])
{
    float r = 0.f;
    int   i;

    for (i = 0; i < 4; ++i)
        r += meta[i] * (1.f - fabsf(meta[i] - ch[i]));
    return clamp01(r * 0.25f);
}

int cos_consciousness_init(struct cos_consciousness_state *cs)
{
    if (!cs)
        return -1;
    memset(cs, 0, sizeof(*cs));
    cs->timestamp_ms = wall_ms();
    return 0;
}

int cos_consciousness_measure(struct cos_consciousness_state *cs,
                              float                         sigma_combined,
                              const float                  *sigma_channels,
                              const float                  *meta_channels,
                              float                         prev_sigma)
{
    clock_t t0 = clock(), t1;
    double  elapsed_ms;

    float ch[4];
    float meta[4];
    float sm_err;
    int   i;

    if (!cs || !sigma_channels)
        return -1;

    for (i = 0; i < 4; ++i)
        ch[i] = clamp01(sigma_channels[i]);

    if (meta_channels) {
        for (i = 0; i < 4; ++i)
            meta[i] = clamp01(meta_channels[i]);
    } else {
        for (i = 0; i < 4; ++i)
            meta[i] = sigma_combined;
    }

    cs->phi_proxy = clamp01(1.f - clamp01(sigma_combined));

    cs->integration     = channel_integration(ch);
    cs->differentiation = channel_differentiation(ch);
    cs->self_model_accuracy =
        meta_channels ? self_model_acc(ch, meta) : 0.5f;

    cs->broadcast_strength =
        0.5f * cs->integration + 0.5f * (1.f - fabsf(ch[0] - ch[3]));

    t1 = clock();
    elapsed_ms =
        (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
    if (elapsed_ms < 0.0)
        elapsed_ms = 0.0;
    if (elapsed_ms > 50.0)
        elapsed_ms = 50.0;
    cs->access_latency_ms = (float)elapsed_ms;

    cs->meta_awareness =
        meta_channels ? meta_aware_score(ch, meta) : 0.35f;

    sm_err = fabsf(ch[1] - ch[2]);
    cs->loop_detection =
        clamp01(1.f - sm_err * 2.f);

    cs->temporal_continuity =
        clamp01(1.f - fabsf(sigma_combined - prev_sigma));

    cs->consciousness_index =
        0.30f * cs->phi_proxy + 0.20f * cs->integration
        + 0.20f * cs->differentiation + 0.20f * cs->self_model_accuracy
        + 0.10f * cs->temporal_continuity;

    cs->consciousness_index =
        clamp01(cs->consciousness_index);

    cs->level            = cos_consciousness_classify_level(cs);
    cs->timestamp_ms     = wall_ms();
    return 0;
}

int cos_consciousness_classify_level(
    const struct cos_consciousness_state *cs)
{
    float s;

    if (!cs)
        return 0;

    s = 1.f - cs->phi_proxy;
    if (s >= 0.999f)
        return 0;
    if (s < 0.1f)
        return 4;
    if (s < 0.2f)
        return 3;
    if (s < 0.4f)
        return 2;
    if (s < 0.7f)
        return 1;
    return 0;
}

char *cos_consciousness_to_json(const struct cos_consciousness_state *cs)
{
    char *out;
    int   n;

    if (!cs)
        return NULL;

    out = (char *)malloc(1024);
    if (!out)
        return NULL;

    n = snprintf(out, 1024,
                 "{\"phi_proxy\":%.6f,\"integration\":%.6f,"
                 "\"differentiation\":%.6f,\"self_model_accuracy\":%.6f,"
                 "\"broadcast_strength\":%.6f,\"access_latency_ms\":%.6f,"
                 "\"meta_awareness\":%.6f,\"loop_detection\":%.6f,"
                 "\"temporal_continuity\":%.6f,\"consciousness_index\":%.6f,"
                 "\"level\":%d,\"timestamp_ms\":%lld}",
                 (double)cs->phi_proxy, (double)cs->integration,
                 (double)cs->differentiation, (double)cs->self_model_accuracy,
                 (double)cs->broadcast_strength, (double)cs->access_latency_ms,
                 (double)cs->meta_awareness, (double)cs->loop_detection,
                 (double)cs->temporal_continuity,
                 (double)cs->consciousness_index, cs->level,
                 (long long)cs->timestamp_ms);
    if (n <= 0 || n >= 1024) {
        free(out);
        return NULL;
    }
    return out;
}

void cos_consciousness_print(const struct cos_consciousness_state *cs,
                             FILE                               *fp)
{
    if (!cs || !fp)
        return;
    fprintf(fp,
            "consciousness_index: %.4f  level: %d\n"
            "  phi_proxy: %.4f  integration: %.4f  differentiation: %.4f\n"
            "  self_model_accuracy: %.4f  temporal_continuity: %.4f\n"
            "  meta_awareness: %.4f  loop_detection: %.4f\n"
            "  broadcast: %.4f  access_latency_ms: %.3f\n",
            (double)cs->consciousness_index, cs->level,
            (double)cs->phi_proxy, (double)cs->integration,
            (double)cs->differentiation, (double)cs->self_model_accuracy,
            (double)cs->temporal_continuity, (double)cs->meta_awareness,
            (double)cs->loop_detection, (double)cs->broadcast_strength,
            (double)cs->access_latency_ms);
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int cm_fail(const char *m)
{
    fprintf(stderr, "consciousness_meter self-test: %s\n", m);
    return 1;
}
#endif

int cos_consciousness_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_consciousness_state cs;
    float                          ch[4]   = {0.2f, 0.25f, 0.22f, 0.21f};
    float                          meta[4] = {0.21f, 0.24f, 0.23f, 0.20f};
    char                          *js;

    if (cos_consciousness_init(&cs) != 0)
        return cm_fail("init");

    if (cos_consciousness_measure(&cs, 0.22f, ch, meta, 0.30f) != 0)
        return cm_fail("measure");

    if (cos_consciousness_classify_level(&cs) < 1)
        return cm_fail("level");

    js = cos_consciousness_to_json(&cs);
    if (!js || !strstr(js, "consciousness_index"))
        return cm_fail("json");
    free(js);

    fprintf(stderr, "consciousness_meter self-test: OK\n");
#endif
    return 0;
}
