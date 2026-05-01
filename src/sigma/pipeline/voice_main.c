/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-voice demo — runs three scripted turns through the driver:
 *   1. clean Finnish query            → admit → synthesise
 *   2. noisy capture (σ_stt = 0.85)   → ask user to repeat
 *   3. low-confidence response        → admit → skip synthesis
 *
 * Emits a stable JSON report the check script pins against.
 */
#define _GNU_SOURCE
#include "voice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void esc(const char *s, char *dst, size_t cap) {
    size_t j = 0;
    if (!s) { if (cap) dst[0] = '\0'; return; }
    for (size_t i = 0; s[i] && j + 2 < cap; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = (char)c; }
        else if (c == '\n')        { dst[j++] = '\\'; dst[j++] = 'n';      }
        else if (c == '\t')        { dst[j++] = '\\'; dst[j++] = 't';      }
        else if (c < 0x20)         {
            int w = snprintf(dst + j, cap - j, "\\u%04x", c);
            if (w < 0 || (size_t)w >= cap - j) break;
            j += (size_t)w;
        } else dst[j++] = (char)c;
    }
    if (j < cap) dst[j] = '\0'; else if (cap) dst[cap - 1] = '\0';
}

/* Fake σ-pipeline responder: echo the input with a canned prefix;
 * σ_response = 0.25 on anything that mentions "sigma", 0.85
 * otherwise so we hit both τ-response branches. */
static int responder(void *caller, const char *in, char *out,
                     size_t cap, float *sigma) {
    (void)caller;
    if (strstr(in, "sigma") || strstr(in, "rag") || strstr(in, "athena")) {
        snprintf(out, cap, "kyllä — %s on osa creation os -pipelinea", in);
        *sigma = 0.25f;
    } else {
        snprintf(out, cap, "en ole varma: %s", in);
        *sigma = 0.85f;
    }
    return 0;
}

typedef struct {
    const char *label;
    const char *transcript;      /* scripted STT output */
    float       stt_sigma;
    const char *synth_input;     /* used only for logging */
} scripted_turn_t;

int main(void) {
    scripted_turn_t turns[] = {
        { "clean_query",  "mitä on sigma rag",      0.12f, "mitä on sigma rag" },
        { "noisy_capture","...",                    0.85f, "(truncated noise)" },
        { "low_conf_resp","mitä on ilma",           0.20f, "mitä on ilma"     },
    };
    const int NT = (int)(sizeof turns / sizeof turns[0]);

    cos_voice_config_t cfg = {
        .tau_stt       = COS_VOICE_TAU_STT_DEFAULT,
        .tau_response  = COS_VOICE_TAU_RESP_DEFAULT,
        .sample_rate   = COS_VOICE_SAMPLE_RATE_DEFAULT,
    };
    uint8_t wav_out[2048];

    int self_rc = cos_voice_self_test();

    printf("{\n");
    printf("  \"kernel\": \"sigma_voice\",\n");
    printf("  \"self_test\": %d,\n", self_rc);
    printf("  \"tau_stt\": %.3f,\n",     (double)cfg.tau_stt);
    printf("  \"tau_response\": %.3f,\n", (double)cfg.tau_response);
    printf("  \"sample_rate\": %d,\n", cfg.sample_rate);
    printf("  \"backend\": \"sim\",\n");
    printf("  \"turns\": [\n");

    for (int i = 0; i < NT; i++) {
        cos_voice_sim_script_t sc = {
            .transcript   = turns[i].transcript,
            .stt_sigma    = turns[i].stt_sigma,
            .synth_prefix = NULL,
        };
        cos_voice_backend_t b = cos_voice_sim_backend(&sc);

        /* The wav payload only matters to the sim backend when
         * transcript is NULL; here we always pass something. */
        const char *fake_wav = "raw-pcm";
        cos_voice_turn_t t;
        int rc = cos_voice_turn(&b, &cfg,
                                (const uint8_t *)fake_wav, strlen(fake_wav),
                                responder, NULL,
                                wav_out, sizeof wav_out, &t);

        char el[128], eitxt[2048], erep[2048];
        esc(turns[i].label, el, sizeof el);
        esc(t.input_text,   eitxt, sizeof eitxt);
        esc(t.reply_text,   erep,  sizeof erep);

        printf("    {\n");
        printf("      \"label\": \"%s\",\n", el);
        printf("      \"rc\": %d,\n", rc);
        printf("      \"input\": \"%s\",\n", eitxt);
        printf("      \"sigma_stt\": %.3f,\n", (double)t.sigma_stt);
        printf("      \"admitted\": %s,\n", t.admitted ? "true" : "false");
        printf("      \"reply\": \"%s\",\n", erep);
        printf("      \"sigma_response\": %.3f,\n", (double)t.sigma_response);
        printf("      \"synthesised\": %s,\n", t.synthesised ? "true" : "false");
        printf("      \"repeat\": %s,\n", t.repeat ? "true" : "false");
        printf("      \"wav_in_bytes\": %zu,\n", t.wav_in_bytes);
        printf("      \"wav_out_bytes\": %zu\n", t.wav_out_bytes);
        printf("    }%s\n", (i + 1 < NT) ? "," : "");
    }

    /* Probe the native backend (just to show the discovery API). */
    cos_voice_native_config_t native_cfg = {0};
    /* Typical install paths; absent on CI. */
    strncpy(native_cfg.whisper_bin, "/usr/local/bin/whisper-cli",
            sizeof native_cfg.whisper_bin - 1);
    strncpy(native_cfg.piper_bin,   "/usr/local/bin/piper",
            sizeof native_cfg.piper_bin - 1);
    int native = cos_voice_native_available(&native_cfg);

    printf("  ],\n");
    printf("  \"native_available\": %s\n", native ? "true" : "false");
    printf("}\n");
    return 0;
}
