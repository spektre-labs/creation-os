/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Voice — local speech interface.
 *
 * Speech is the second modality.  Creation OS does *not* call
 * Siri, Alexa, or OpenAI Whisper-API: it shells out to two
 * fully local tools — whisper.cpp for speech-to-text, piper for
 * text-to-speech — and measures σ at both ends of the loop so
 * the pipeline can ask the user to repeat when recognition is
 * shaky.
 *
 *     user speaks ─► whisper.cpp ─► text (σ_stt)
 *                                    │
 *                               σ-pipeline
 *                                    │
 *                   piper ◄──── text (σ_response)
 *                   │
 *                   user hears
 *
 * If σ_stt is high ("mitä sanoit?") we do not pass the transcript
 * into the LLM at all; we synthesise a reply asking the user to
 * repeat.  The core contracts:
 *
 *   σ_stt ∈ [0, 1]         clipped from whisper's log-prob or a
 *                          caller-supplied confidence
 *   σ_response ∈ [0, 1]    from the σ-pipeline
 *   recognition gate:      admit iff σ_stt ≤ τ_stt (default 0.50)
 *   synthesis gate:        synthesise iff σ_response ≤ τ_response
 *                          (default 0.70)
 *
 * This header defines the abstract interface and the
 * deterministic simulation backend so tests can cover every
 * branch without whisper/piper installed.  A second "native"
 * backend (not compiled by default) uses execvp() to drive
 * whisper.cpp and piper binaries via pipes.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_VOICE_H
#define COS_SIGMA_VOICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_VOICE_TEXT_MAX           2048
#define COS_VOICE_WAV_MAX            (1u << 20)   /* 1 MiB capture      */
#define COS_VOICE_TAU_STT_DEFAULT    0.50f
#define COS_VOICE_TAU_RESP_DEFAULT   0.70f
#define COS_VOICE_SAMPLE_RATE_DEFAULT 16000

enum cos_voice_status {
    COS_VOICE_OK        =  0,
    COS_VOICE_ERR_ARG   = -1,
    COS_VOICE_ERR_STT   = -2,
    COS_VOICE_ERR_TTS   = -3,
    COS_VOICE_REPEAT    = -4,   /* σ_stt above τ_stt; ask to repeat */
};

/* Abstract STT / TTS backend.  Each function returns 0 on
 * success.  `caller` is an opaque pointer for per-backend state. */
typedef struct {
    void       *caller;
    /* wav: PCM bytes, sz: size.  Writes transcript into `text`
     * (≤ text_cap bytes including NUL) and σ_stt into *sigma. */
    int  (*stt)(void *caller,
                const uint8_t *wav, size_t sz,
                char *text, size_t text_cap,
                float *sigma_out);
    /* text -> wav PCM.  Writes capped bytes into wav_out (≤
     * wav_cap), real size into *wav_sz_out.  Returns 0 on success. */
    int  (*tts)(void *caller,
                const char *text,
                uint8_t *wav_out, size_t wav_cap,
                size_t *wav_sz_out);
} cos_voice_backend_t;

typedef struct {
    float    tau_stt;
    float    tau_response;
    int      sample_rate;
} cos_voice_config_t;

/* Downstream σ-pipeline contract: given the recognised text,
 * produce a reply and a σ_response.  The driver is the one that
 * decides whether to synthesise or not. */
typedef int (*cos_voice_respond_fn)(void *caller,
                                    const char *input_text,
                                    char *reply_out, size_t reply_cap,
                                    float *sigma_response);

typedef struct {
    char   input_text[COS_VOICE_TEXT_MAX];
    float  sigma_stt;
    int    admitted;          /* 1 = passed stt gate                  */
    char   reply_text[COS_VOICE_TEXT_MAX];
    float  sigma_response;
    int    synthesised;       /* 1 = tts ran                          */
    size_t wav_in_bytes;
    size_t wav_out_bytes;
    int    repeat;            /* 1 = asked user to repeat             */
} cos_voice_turn_t;

int cos_voice_turn(const cos_voice_backend_t *backend,
                   const cos_voice_config_t  *cfg,
                   const uint8_t *wav_in, size_t wav_in_sz,
                   cos_voice_respond_fn respond,
                   void *respond_caller,
                   uint8_t *wav_out, size_t wav_out_cap,
                   cos_voice_turn_t *turn);

/* -------- Simulation backend --------------------------------- */

typedef struct {
    /* Scripted outputs the caller wants the simulated STT/TTS to
     * return.  If `transcript` is NULL we read it from the WAV
     * ASCII (WAV is our test fixture — pass plain text bytes).
     * `stt_sigma` is clipped into [0, 1]. */
    const char *transcript;
    float       stt_sigma;
    /* If non-NULL, overrides the synthesised bytes; otherwise the
     * sim writes the plain-text reply as WAV-ASCII. */
    const char *synth_prefix;
} cos_voice_sim_script_t;

cos_voice_backend_t cos_voice_sim_backend(cos_voice_sim_script_t *script);

/* -------- Native whisper.cpp / piper backend (descriptor only) --
 *
 * The "native" backend execs whisper.cpp / piper binaries.  This
 * header only declares the descriptor; the implementation lives
 * behind a runtime probe so the kernel compiles even when the
 * binaries are absent.  When neither binary is in $PATH, calling
 * the backend returns COS_VOICE_ERR_STT and the caller is expected
 * to fall back to the simulation backend for tests.
 */
typedef struct {
    char whisper_bin[256];
    char whisper_model[512];
    char piper_bin[256];
    char piper_model[512];
} cos_voice_native_config_t;

cos_voice_backend_t cos_voice_native_backend(
    cos_voice_native_config_t *cfg);

/* Probe helper: returns 1 if both binaries (whisper.cpp and
 * piper) appear to exist at the configured paths. */
int cos_voice_native_available(const cos_voice_native_config_t *cfg);

/* -------- Self-test ----------------------------------------- */
int cos_voice_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_VOICE_H */
