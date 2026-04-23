/*
 * Sigma-spike engine implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "spike_engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static int64_t cos_spike_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int cos_spike_engine_init(struct cos_spike_engine *e)
{
    if (e == NULL)
        return -1;
    memset(e, 0, sizeof(*e));
    e->channels[0].threshold = 0.05f; /* logprob */
    e->channels[1].threshold = 0.05f; /* entropy */
    e->channels[2].threshold = 0.05f; /* perplexity */
    e->channels[3].threshold = 0.10f; /* consistency */
    e->channels[4].threshold = 0.10f; /* perception */
    e->channels[5].threshold = 0.15f; /* self */
    e->channels[6].threshold = 0.10f; /* social */
    e->channels[7].threshold = 0.10f; /* situational */
    return 0;
}

int cos_spike_check(struct cos_spike_engine *e, const float *new_values,
                    int *fired_channels, int *n_fired)
{
    int i;

    if (e == NULL || new_values == NULL || fired_channels == NULL
        || n_fired == NULL)
        return -1;

    *n_fired = 0;
    if (!e->primed) {
        int64_t now = cos_spike_now_ms();
        for (i = 0; i < COS_SPIKE_CHANNELS; ++i) {
            e->channels[i].value            = new_values[i];
            e->channels[i].last_fired_value = new_values[i];
            e->channels[i].last_fired_ms    = now;
            fired_channels[i]               = 0;
        }
        e->primed = 1;
        return 0;
    }

    for (i = 0; i < COS_SPIKE_CHANNELS; ++i) {
        float dv = fabsf(new_values[i] - e->channels[i].value);
        fired_channels[i] = 0;
        if (dv > e->channels[i].threshold) {
            fired_channels[i] = 1;
            (*n_fired)++;
            e->channels[i].fire_count++;
            e->channels[i].last_fired_value = new_values[i];
            e->channels[i].last_fired_ms    = cos_spike_now_ms();
            e->channels[i].value            = new_values[i];
        } else {
            e->channels[i].suppress_count++;
            e->channels[i].value = new_values[i];
        }
    }

    if (*n_fired > 0)
        e->total_fires++;
    else
        e->total_suppressed++;

    {
        int sum = e->total_fires + e->total_suppressed;
        e->energy_saved_ratio =
            sum > 0 ? (float)e->total_suppressed / (float)sum : 0.f;
    }

    return 0;
}

void cos_spike_engine_stats(const struct cos_spike_engine *e,
                            float *energy_saved, int *total_fires,
                            int *total_suppressed)
{
    if (energy_saved)
        *energy_saved = e != NULL ? e->energy_saved_ratio : 0.f;
    if (total_fires)
        *total_fires = e != NULL ? e->total_fires : 0;
    if (total_suppressed)
        *total_suppressed = e != NULL ? e->total_suppressed : 0;
}

void cos_spike_fill_channels_from_speculative(float predicted_sigma,
                                              float confidence,
                                              float *out8)
{
    float s = predicted_sigma;
    float c = confidence;
    int   i;

    if (out8 == NULL)
        return;
    if (s < 0.f)
        s = 0.f;
    if (s > 1.f)
        s = 1.f;
    if (c < 0.f)
        c = 0.f;
    if (c > 1.f)
        c = 1.f;

    out8[0] = s;
    out8[1] = s + 0.02f * (1.f - c);
    out8[2] = s - 0.02f * (1.f - c);
    out8[3] = s * (0.85f + 0.15f * c);
    out8[4] = s + 0.03f * (1.f - c);
    out8[5] = s + 0.05f * (1.f - c);
    out8[6] = s - 0.03f * (1.f - c);
    out8[7] = s - 0.05f * (1.f - c);
    for (i = 0; i < COS_SPIKE_CHANNELS; ++i) {
        if (out8[i] < 0.f)
            out8[i] = 0.f;
        if (out8[i] > 1.f)
            out8[i] = 1.f;
    }
}

void cos_spike_engine_print(FILE *fp, const struct cos_spike_engine *e)
{
    int i;

    if (fp == NULL || e == NULL)
        return;
    fprintf(fp,
            "sigma-spike engine: turns_with_spike=%d turns_suppressed=%d "
            "energy_saved_ratio=%.4f primed=%d\n",
            e->total_fires, e->total_suppressed,
            (double)e->energy_saved_ratio, e->primed);
    for (i = 0; i < COS_SPIKE_CHANNELS; ++i) {
        fprintf(fp,
                "  ch%d value=%.4f τ=%.4f last_fired=%.4f fires=%d "
                "suppress=%d\n",
                i, (double)e->channels[i].value,
                (double)e->channels[i].threshold,
                (double)e->channels[i].last_fired_value,
                e->channels[i].fire_count, e->channels[i].suppress_count);
    }
}

#define COS_SPIKE_SNAP_MAGIC "COS_SPIKE_SNAPSHOT_V1\n"

int cos_spike_engine_snapshot_write(const struct cos_spike_engine *e,
                                    const char *path)
{
    FILE *fp;
    int   i;

    if (e == NULL || path == NULL || path[0] == '\0')
        return -1;
    fp = fopen(path, "w");
    if (fp == NULL)
        return -2;
    fputs(COS_SPIKE_SNAP_MAGIC, fp);
    fprintf(fp, "total_fires %d\n", e->total_fires);
    fprintf(fp, "total_suppressed %d\n", e->total_suppressed);
    fprintf(fp, "energy_saved_ratio %.9g\n", (double)e->energy_saved_ratio);
    fprintf(fp, "primed %d\n", e->primed);
    for (i = 0; i < COS_SPIKE_CHANNELS; ++i) {
        const struct cos_spike_channel *ch = &e->channels[i];
        fprintf(fp, "ch %d %.9g %.9g %.9g %lld %d %d\n", i,
                (double)ch->value, (double)ch->threshold,
                (double)ch->last_fired_value,
                (long long)ch->last_fired_ms, ch->fire_count,
                ch->suppress_count);
    }
    fclose(fp);
    return 0;
}

int cos_spike_engine_snapshot_read(struct cos_spike_engine *e,
                                   const char *path)
{
    FILE *fp;
    char  magic[64];
    int   i, ver, tf, ts, pr;
    float esr;

    if (e == NULL || path == NULL || path[0] == '\0')
        return -1;
    fp = fopen(path, "r");
    if (fp == NULL)
        return -2;
    if (fgets(magic, (int)sizeof magic, fp) == NULL) {
        fclose(fp);
        return -3;
    }
    if (strcmp(magic, COS_SPIKE_SNAP_MAGIC) != 0) {
        fclose(fp);
        return -4;
    }
    ver = fscanf(fp, "total_fires %d\n", &tf);
    if (ver != 1) {
        fclose(fp);
        return -5;
    }
    ver = fscanf(fp, "total_suppressed %d\n", &ts);
    if (ver != 1) {
        fclose(fp);
        return -5;
    }
    ver = fscanf(fp, "energy_saved_ratio %f\n", &esr);
    if (ver != 1) {
        fclose(fp);
        return -5;
    }
    ver = fscanf(fp, "primed %d\n", &pr);
    if (ver != 1) {
        fclose(fp);
        return -5;
    }
    e->total_fires       = tf;
    e->total_suppressed  = ts;
    e->energy_saved_ratio = esr;
    e->primed            = pr;
    for (i = 0; i < COS_SPIKE_CHANNELS; ++i) {
        int          idx;
        double       v, th, lf;
        long long    lms;
        int          fc, sc;
        struct cos_spike_channel *ch = &e->channels[i];
        ver = fscanf(fp, "ch %d %lf %lf %lf %lld %d %d\n", &idx, &v, &th,
                     &lf, &lms, &fc, &sc);
        if (ver != 7 || idx != i) {
            fclose(fp);
            return -6;
        }
        ch->value            = (float)v;
        ch->threshold        = (float)th;
        ch->last_fired_value = (float)lf;
        ch->last_fired_ms    = (int64_t)lms;
        ch->fire_count       = fc;
        ch->suppress_count   = sc;
    }
    fclose(fp);
    return 0;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int fail(const char *msg)
{
    fprintf(stderr, "spike_engine self-test: %s\n", msg);
    return 1;
}
#endif

int cos_spike_engine_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_spike_engine e;
    float                   v1[8], v2[8];
    int                     fired[8], nf;
    float                   es;
    int                     tf, ts;
    char                    tmp[] = "/tmp/cos_spike_selftest_XXXXXX";
    int                     fd;
    struct cos_spike_engine e2;

    if (cos_spike_engine_init(&e) != 0)
        return fail("init");

    cos_spike_fill_channels_from_speculative(0.2f, 0.9f, v1);
    if (fabsf(v1[0] - 0.2f) > 1e-3f)
        return fail("fill primary");

    if (cos_spike_check(&e, v1, fired, &nf) != 0 || nf != 0)
        return fail("prime should not count spikes");

    cos_spike_fill_channels_from_speculative(0.5f, 0.5f, v2);
    if (cos_spike_check(&e, v2, fired, &nf) != 0)
        return fail("check");
    if (nf <= 0)
        return fail("expected channel spike after large drift");

    cos_spike_engine_stats(&e, &es, &tf, &ts);
    if (tf < 1 || ts < 0)
        return fail("stats fires");

    fd = mkstemp(tmp);
    if (fd < 0)
        return fail("mkstemp");
    close(fd);
    if (cos_spike_engine_snapshot_write(&e, tmp) != 0)
        return fail("snapshot write");
    if (cos_spike_engine_init(&e2) != 0)
        return fail("init2");
    if (cos_spike_engine_snapshot_read(&e2, tmp) != 0)
        return fail("snapshot read");
    if (e2.total_fires != e.total_fires)
        return fail("roundtrip fires");
    remove(tmp);

    if (cos_spike_engine_init(&e) != 0)
        return fail("init3");
    cos_spike_fill_channels_from_speculative(0.25f, 1.0f, v1);
    (void)cos_spike_check(&e, v1, fired, &nf);
    (void)cos_spike_check(&e, v1, fired, &nf);
    if (nf != 0)
        return fail("identical vectors should not spike");

    fprintf(stderr, "spike_engine self-test: OK\n");
#endif
    return 0;
}
