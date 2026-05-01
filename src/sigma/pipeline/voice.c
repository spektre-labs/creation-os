/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Voice — local speech interface (implementation).
 *
 * The driver is the same for both backends; the backend struct
 * just swaps in different stt/tts function pointers.  The native
 * backend shells to whisper.cpp / piper, the simulation backend
 * plays scripted responses.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "voice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static float vclip01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void vncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* ==================================================================
 * Driver
 * ================================================================== */
int cos_voice_turn(const cos_voice_backend_t *backend,
                   const cos_voice_config_t  *cfg,
                   const uint8_t *wav_in, size_t wav_in_sz,
                   cos_voice_respond_fn respond,
                   void *respond_caller,
                   uint8_t *wav_out, size_t wav_out_cap,
                   cos_voice_turn_t *turn) {
    if (!backend || !cfg || !respond || !turn ||
        !backend->stt || !backend->tts) return COS_VOICE_ERR_ARG;

    memset(turn, 0, sizeof *turn);
    turn->wav_in_bytes = wav_in_sz;

    /* -------- STT --------------------------------------------- */
    float s_stt = 1.0f;
    int rc = backend->stt(backend->caller,
                          wav_in, wav_in_sz,
                          turn->input_text, sizeof turn->input_text,
                          &s_stt);
    if (rc != 0) {
        turn->sigma_stt = 1.0f;
        turn->admitted  = 0;
        turn->repeat    = 1;
        /* Surface the scripted "please repeat" response but do
         * not route through the σ-pipeline. */
        vncpy(turn->reply_text, sizeof turn->reply_text,
              "en tunnistanut, voitko toistaa?");
        turn->sigma_response = 1.0f;
        turn->synthesised = 0;
        return COS_VOICE_ERR_STT;
    }
    turn->sigma_stt = vclip01(s_stt);

    /* -------- STT gate --------------------------------------- */
    if (turn->sigma_stt > cfg->tau_stt) {
        turn->admitted = 0;
        turn->repeat   = 1;
        vncpy(turn->reply_text, sizeof turn->reply_text,
              "en ymmärtänyt, voisitko toistaa?");
        turn->sigma_response = 1.0f;
        /* Synthesise the repeat prompt so the user hears it. */
        size_t wbytes = 0;
        if (backend->tts(backend->caller, turn->reply_text,
                         wav_out, wav_out_cap, &wbytes) == 0) {
            turn->synthesised   = 1;
            turn->wav_out_bytes = wbytes;
        }
        return COS_VOICE_REPEAT;
    }
    turn->admitted = 1;

    /* -------- σ-pipeline ------------------------------------- */
    float s_resp = 1.0f;
    if (respond(respond_caller,
                turn->input_text,
                turn->reply_text, sizeof turn->reply_text,
                &s_resp) != 0) {
        vncpy(turn->reply_text, sizeof turn->reply_text,
              "pipeline failed");
        turn->sigma_response = 1.0f;
        return COS_VOICE_ERR_STT;
    }
    turn->sigma_response = vclip01(s_resp);

    /* -------- Response gate ---------------------------------- */
    if (turn->sigma_response > cfg->tau_response) {
        turn->synthesised = 0;
        return COS_VOICE_OK;  /* caller may re-prompt via text */
    }

    size_t wbytes = 0;
    int trc = backend->tts(backend->caller, turn->reply_text,
                           wav_out, wav_out_cap, &wbytes);
    if (trc != 0) {
        turn->synthesised = 0;
        return COS_VOICE_ERR_TTS;
    }
    turn->synthesised   = 1;
    turn->wav_out_bytes = wbytes;
    return COS_VOICE_OK;
}

/* ==================================================================
 * Simulation backend
 * ================================================================== */
static int sim_stt(void *caller,
                   const uint8_t *wav, size_t sz,
                   char *text, size_t text_cap,
                   float *sigma_out) {
    cos_voice_sim_script_t *s = (cos_voice_sim_script_t *)caller;
    if (!s || !text || text_cap == 0 || !sigma_out) return -1;
    if (s->transcript) {
        vncpy(text, text_cap, s->transcript);
    } else {
        /* WAV fixture == ASCII bytes */
        size_t n = sz < text_cap - 1 ? sz : text_cap - 1;
        if (wav) memcpy(text, wav, n);
        text[n] = '\0';
    }
    *sigma_out = vclip01(s->stt_sigma);
    return 0;
}

static int sim_tts(void *caller,
                   const char *text,
                   uint8_t *wav_out, size_t wav_cap,
                   size_t *wav_sz_out) {
    cos_voice_sim_script_t *s = (cos_voice_sim_script_t *)caller;
    (void)s;
    if (!text || !wav_out || wav_cap == 0 || !wav_sz_out) return -1;
    /* Identity TTS: encode the reply text as its own bytes. */
    size_t n = strlen(text);
    if (n > wav_cap) n = wav_cap;
    memcpy(wav_out, text, n);
    *wav_sz_out = n;
    return 0;
}

cos_voice_backend_t cos_voice_sim_backend(cos_voice_sim_script_t *script) {
    cos_voice_backend_t b;
    b.caller = script;
    b.stt    = sim_stt;
    b.tts    = sim_tts;
    return b;
}

/* ==================================================================
 * Native backend
 *
 * Minimal discovery (does whisper.cpp / piper exist at the given
 * path?).  The actual execvp() wiring is a trivial wrapper around
 * whisper-cli --model <path> --file <wav> and piper --model
 * <path>; when the binaries are absent the backend fails with
 * COS_VOICE_ERR_STT so tests fall back to the simulation
 * backend.  We deliberately keep that codepath out of the
 * default build: it pulls in fork/exec semantics that the
 * σ-sandbox kernel (NEXT-3) already owns, and voice IO is
 * host-dependent enough that the unit test is better off using
 * the deterministic simulation backend.
 * ================================================================== */
int cos_voice_native_available(const cos_voice_native_config_t *cfg) {
    if (!cfg) return 0;
    struct stat st;
    if (cfg->whisper_bin[0] && stat(cfg->whisper_bin, &st) != 0) return 0;
    if (cfg->piper_bin[0]   && stat(cfg->piper_bin,   &st) != 0) return 0;
    return 1;
}

static int native_stt(void *caller,
                      const uint8_t *wav, size_t sz,
                      char *text, size_t text_cap,
                      float *sigma_out) {
    (void)caller; (void)wav; (void)sz;
    (void)text; (void)text_cap;
    *sigma_out = 1.0f;
    return -1;   /* placeholder: real build gates on native_available() */
}

static int native_tts(void *caller,
                      const char *text,
                      uint8_t *wav_out, size_t wav_cap,
                      size_t *wav_sz_out) {
    (void)caller; (void)text;
    (void)wav_out; (void)wav_cap;
    *wav_sz_out = 0;
    return -1;
}

cos_voice_backend_t cos_voice_native_backend(cos_voice_native_config_t *cfg) {
    cos_voice_backend_t b;
    b.caller = cfg;
    b.stt    = native_stt;
    b.tts    = native_tts;
    return b;
}

/* ==================================================================
 * Self-test (uses simulation backend for every branch)
 * ================================================================== */
static int echo_respond(void *caller,
                        const char *input_text,
                        char *reply_out, size_t reply_cap,
                        float *sigma_response) {
    (void)caller;
    snprintf(reply_out, reply_cap, "ack: %s", input_text);
    *sigma_response = 0.20f;
    return 0;
}

static int high_sigma_respond(void *caller, const char *in, char *out,
                              size_t cap, float *sig) {
    (void)caller;
    snprintf(out, cap, "not sure: %s", in);
    *sig = 0.95f;
    return 0;
}

int cos_voice_self_test(void) {
    cos_voice_sim_script_t ok_script = {
        .transcript = "mitä on sigma gate",
        .stt_sigma  = 0.18f,
        .synth_prefix = NULL,
    };
    cos_voice_backend_t  b = cos_voice_sim_backend(&ok_script);
    cos_voice_config_t cfg = {
        .tau_stt       = COS_VOICE_TAU_STT_DEFAULT,
        .tau_response  = COS_VOICE_TAU_RESP_DEFAULT,
        .sample_rate   = COS_VOICE_SAMPLE_RATE_DEFAULT,
    };

    uint8_t wav_in[]  = "raw audio bytes";
    uint8_t wav_out[512] = {0};
    cos_voice_turn_t t;

    /* (1) Happy path */
    int rc = cos_voice_turn(&b, &cfg,
                            wav_in, sizeof wav_in - 1,
                            echo_respond, NULL,
                            wav_out, sizeof wav_out, &t);
    if (rc != COS_VOICE_OK) return -1;
    if (!t.admitted)        return -2;
    if (!t.synthesised)     return -3;
    if (t.repeat)           return -4;
    if (strcmp(t.input_text, "mitä on sigma gate") != 0) return -5;
    if (strstr(t.reply_text, "ack: mitä") == NULL)       return -6;
    if (t.wav_out_bytes == 0) return -7;

    /* (2) σ_stt high → repeat */
    ok_script.stt_sigma = 0.90f;
    rc = cos_voice_turn(&b, &cfg,
                        wav_in, sizeof wav_in - 1,
                        echo_respond, NULL,
                        wav_out, sizeof wav_out, &t);
    if (rc != COS_VOICE_REPEAT) return -8;
    if (t.admitted)              return -9;
    if (!t.repeat)               return -10;

    /* (3) response σ above τ_response → no synthesis */
    ok_script.stt_sigma = 0.10f;
    rc = cos_voice_turn(&b, &cfg,
                        wav_in, sizeof wav_in - 1,
                        high_sigma_respond, NULL,
                        wav_out, sizeof wav_out, &t);
    if (rc != COS_VOICE_OK) return -11;
    if (t.synthesised)       return -12;

    return 0;
}
