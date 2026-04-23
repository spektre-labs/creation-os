/*
 * cos consciousness — operational consciousness index sample + log.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_consciousness_cli.h"

#include "../sigma/awareness_log.h"
#include "../sigma/consciousness_meter.h"
#include "../sigma/state_ledger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int argv_skip_cmd(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "consciousness") == 0)
        return 2;
    return 1;
}

static void fill_from_ledger(float *sigma_combined, float ch[4], int *ok)
{
    struct cos_state_ledger L;
    char                   path[768];

    *ok = 0;
    if (cos_state_ledger_default_path(path, sizeof path) != 0)
        return;
    cos_state_ledger_init(&L);
    if (cos_state_ledger_load(&L, path) != 0)
        return;

    if (L.sigma_mean_session >= 0.f && L.sigma_mean_session <= 1.f) {
        int i;

        *sigma_combined = L.sigma_mean_session;
        for (i = 0; i < 4; ++i)
            ch[i] = L.sigma_mean_session;
        *ok = 1;
    }
}

int cos_consciousness_main(int argc, char **argv)
{
    struct cos_consciousness_state cs;
    float                          sigma_c = 0.5f;
    float                          ch[4]   = {0.5f, 0.5f, 0.5f, 0.5f};
    float                          meta[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float                          prev    = 0.5f;
    int                            base    = argv_skip_cmd(argc, argv);
    int                            i;
    int                            ld_ok   = 0;
    const char                    *env_ch;

    if (argc < base) {
        fputs("usage: cos consciousness [--log|--level|--detail]\n", stderr);
        return 2;
    }

    if (argc > base
        && (strcmp(argv[base], "--help") == 0 || strcmp(argv[base], "-h") == 0)) {
        fputs(
            "cos consciousness — operational σ-integration index\n"
            "  (quantitative index — not a claim of phenomenal consciousness)\n",
            stdout);
        return 0;
    }

    if (argc == base)
        goto run_measure;

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--log") == 0) {
            float trend;
            int   n;

            cos_awareness_close();
            if (cos_awareness_trend(&trend, &n) != 0)
                fputs("awareness trend unavailable\n", stderr);
            else
                printf("samples=%d  Δconsciousness_index=%.6f\n", n,
                       (double)trend);
            return 0;
        }
    }

run_measure:

    fill_from_ledger(&sigma_c, ch, &ld_ok);
    if (ld_ok)
        prev = sigma_c;

    env_ch = getenv("COS_CONSCIOUSNESS_CHANNELS");
    if (env_ch != NULL && env_ch[0]) {
        float a, b, c, d;
        if (sscanf(env_ch, "%f,%f,%f,%f", &a, &b, &c, &d) == 4) {
            ch[0] = a;
            ch[1] = b;
            ch[2] = c;
            ch[3] = d;
            sigma_c = (a + b + c + d) * 0.25f;
        }
    }

    cos_consciousness_init(&cs);
    if (cos_consciousness_measure(&cs, sigma_c, ch, meta, prev) != 0) {
        fputs("measure failed\n", stderr);
        return 1;
    }

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--level") == 0) {
            printf("%d\n", cs.level);
            return 0;
        }
        if (strcmp(argv[i], "--detail") == 0) {
            cos_consciousness_print(&cs, stdout);
            {
                char *js = cos_consciousness_to_json(&cs);
                if (js) {
                    fputs(js, stdout);
                    fputc('\n', stdout);
                    free(js);
                }
            }
            return 0;
        }
    }

    printf("consciousness_index %.6f  level %d\n",
           (double)cs.consciousness_index, cs.level);

    if (cos_awareness_open() == 0)
        (void)cos_awareness_log(&cs);

    return 0;
}

#ifdef COS_CONSCIOUSNESS_CLI_MAIN
int main(int argc, char **argv)
{
    return cos_consciousness_main(argc, argv);
}
#endif
