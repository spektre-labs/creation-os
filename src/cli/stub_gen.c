/* Reference stub generator — see stub_gen.h. */

#include "stub_gen.h"

#include "bitnet_server.h"
#include "bitnet_spawn.h"
#include "bitnet_sigma.h"
#include "codex.h"
#include "escalation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One subprocess-sized buffer; cos-chat is single-threaded. */
static char g_cos_chat_bitnet_buf[8192];

/* DEV-5 distill-pair context (escalation.h).  Lives here, not in
 * escalation.c, so that binaries which link stub_gen without the
 * API tier (cos-benchmark, cos-cost) stay libcurl-free. */
cos_escalation_ctx_t g_cos_escalation_ctx = { .student_valid = 0 };

void cos_cli_escalation_record_student(const char *text, float sigma) {
    if (text == NULL) return;
    size_t n = strlen(text);
    if (n >= sizeof g_cos_escalation_ctx.student_text)
        n = sizeof g_cos_escalation_ctx.student_text - 1;
    memcpy(g_cos_escalation_ctx.student_text, text, n);
    g_cos_escalation_ctx.student_text[n] = '\0';
    g_cos_escalation_ctx.student_sigma   = sigma;
    g_cos_escalation_ctx.student_valid   = 1;
}

static int prompt_is_what_is_2_plus_2(const char *prompt) {
    const char *p = prompt;
    if (p == NULL) return 0;
    while (*p == ' ' || *p == '\t') ++p;
    const char *want = "what is 2+2?";
    size_t i = 0;
    for (; want[i]; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        if ((char)c != want[i]) return 0;
    }
    /* Allow trailing whitespace only. */
    while (p[i] == ' ' || p[i] == '\t') ++i;
    return p[i] == '\0';
}

int cos_cli_stub_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur) {
    (void)ctx;
    *out_cost_eur = 0.0001;  /* BitNet-class local cost          */
    if (prompt == NULL) { *out_text = ""; *out_sigma = 1.0f; return 1; }

    if (strncmp(prompt, "low:", 4) == 0) {
        *out_text  = "a confident local answer (stub; real BitNet would fill this in).";
        *out_sigma = 0.05f;
    } else if (strncmp(prompt, "mid:", 4) == 0) {
        *out_text  = "a plateau-σ draft — not confident enough to commit.";
        *out_sigma = 0.55f;
    } else if (strncmp(prompt, "high:", 5) == 0) {
        *out_text  = "I do not know this with enough confidence to answer locally.";
        *out_sigma = 0.92f;
    } else if (strncmp(prompt, "improve:", 8) == 0) {
        float t[] = { 0.55f, 0.45f, 0.35f };
        int r = (round < 3) ? round : 2;
        *out_text  = "an answer that improves with each RETHINK pass.";
        *out_sigma = t[r];
    } else {
        *out_text  = "a default mid-confidence local answer.";
        *out_sigma = 0.30f;
    }
    return 0;
}

/* DEV-4: streaming callback.
 *
 * Installed as the per-token callback when COS_BITNET_STREAM=1.
 * Prints each token's content to stdout immediately (no newline, no
 * buffering beyond stdio's) so the user sees the answer materialise
 * in real time.  When --verbose-style σ-per-token is requested via
 * COS_BITNET_STREAM_VERBOSE=1 we append `[σ=0.12]` after each token
 * so the uncertainty signal is visible during the stream.
 *
 * The callback returns 0 (continue) on every chunk; early-abort
 * would be reported as stopped_limit=1 in the summary. */
typedef struct {
    int verbose;
    int have_printed;
} cos_chat_stream_ctx_t;

static int cos_chat_stream_cb(const char *tok, float sigma,
                              int is_last, void *ctx) {
    cos_chat_stream_ctx_t *s = (cos_chat_stream_ctx_t *)ctx;
    if (tok == NULL || tok[0] == '\0') {
        /* Terminal chunk (stop=true) — server emits empty content. */
        if (is_last && s->have_printed) {
            fputc('\n', stdout);
            fflush(stdout);
        }
        return 0;
    }
    fputs(tok, stdout);
    if (s->verbose) fprintf(stdout, "[σ=%.2f]", (double)sigma);
    fflush(stdout);
    s->have_printed = 1;
    return 0;
}

/* Backend selection (DEV-1 / DEV-4):
 *
 *   COS_BITNET_SERVER=1        → spawn llama-server (bitnet_server.h)
 *                                and measure σ from real per-token
 *                                probs.  Default endpoint is
 *                                /v1/chat/completions (best output
 *                                quality).  Each RETHINK round
 *                                (round > 0) rotates the seed and
 *                                bumps the temperature so the
 *                                sampler explores a different
 *                                trajectory.
 *   COS_BITNET_STREAM=1        → also enable DEV-4 SSE streaming.
 *                                Token text is printed to stdout
 *                                as it arrives (via stream_cb), and
 *                                the pipeline receives the final
 *                                (text, σ) tuple when the stream
 *                                closes.  Uses /completion natively
 *                                because chat streaming drops probs.
 *   CREATION_OS_BITNET_EXE=/…  → legacy one-shot subprocess capture
 *                                with heuristic σ (bitnet_sigma.h).
 *                                Kept for CI parity and for hosts
 *                                without the full server build.
 *   (neither)                  → deterministic stub for self-tests
 *                                that still exercise every pipeline
 *                                branch (ACCEPT / RETHINK / ABSTAIN).
 */
int cos_cli_chat_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur) {
    /* DEV-6: the pipeline threads the loaded codex through ctx.  We
     * install it as the backend's system_prompt so every /v1/chat/
     * completions request (or the manually-prepended streaming
     * prompt) carries Creation OS's canon verbatim.  NULL when
     * --no-codex is set; in that case the backend runs unconditioned. */
    const cos_sigma_codex_t *codex = (const cos_sigma_codex_t *)ctx;
    const char *codex_sysprompt =
        (codex != NULL && codex->bytes != NULL && codex->size > 0)
            ? codex->bytes : NULL;
    if (prompt == NULL) {
        *out_text = "";
        *out_sigma = 1.0f;
        *out_cost_eur = 0.0;
        return 1;
    }

    /* Branch A — llama-server backend (real per-token σ). */
    const char *use_server = getenv("COS_BITNET_SERVER");
    if (use_server != NULL && use_server[0] == '1') {
        cos_bitnet_server_params_t p;
        memset(&p, 0, sizeof(p));
        p.n_predict   = 64;
        p.n_probs     = 5;
        p.seed        = -1;  /* round 0 → server-random (empirically
                              * needed; seed=0 hits a degenerate
                              * BitNet sampler path that emits
                              * "@@@@" with null probs). */
        p.temperature = 0.0f;
        p.system_prompt = codex_sysprompt;
        /* Rotate exploration on RETHINK rounds.  round 0 uses the
         * server default (T=0.8, random seed); subsequent rounds
         * pin a distinct seed + bump temperature so the sampler
         * explores a different trajectory. */
        if (round > 0) {
            p.seed        = 42 + round * 1009;
            p.temperature = 0.8f + 0.15f * (float)round;
        }
        cos_bitnet_server_result_t r;
        int rc;
        const char *use_stream = getenv("COS_BITNET_STREAM");
        if (use_stream != NULL && use_stream[0] == '1') {
            cos_chat_stream_ctx_t sctx = { 0, 0 };
            const char *sv = getenv("COS_BITNET_STREAM_VERBOSE");
            sctx.verbose = (sv != NULL && sv[0] == '1');
            rc = cos_bitnet_server_complete_stream(prompt, &p,
                                                   cos_chat_stream_cb,
                                                   &sctx, &r);
        } else {
            rc = cos_bitnet_server_complete(prompt, &p, &r);
        }
        if (rc == 0) {
            /* Copy text into the module-local buffer so the pipeline
             * can reference it after the next server call clobbers
             * the server's scratch.  `r.text` points at
             * bitnet_server.c's static text_buf, which is fine for
             * the current turn but would race with a later call. */
            size_t n = strlen(r.text);
            if (n >= sizeof(g_cos_chat_bitnet_buf))
                n = sizeof(g_cos_chat_bitnet_buf) - 1;
            memcpy(g_cos_chat_bitnet_buf, r.text, n);
            g_cos_chat_bitnet_buf[n] = '\0';

            *out_text     = g_cos_chat_bitnet_buf;
            *out_sigma    = r.sigma;
            *out_cost_eur = r.cost_eur;
            /* DEV-5: stash for distill pair logging if this round
             * ultimately escalates to a teacher API. */
            cos_cli_escalation_record_student(g_cos_chat_bitnet_buf,
                                              r.sigma);
            return 0;
        }
        /* Server failed → fall through to legacy path.  This keeps
         * the CLI usable when the model file is missing or the
         * server crashed mid-session. */
    }

    /* Branch B — legacy subprocess capture with heuristic σ. */
    const char *exe = getenv("CREATION_OS_BITNET_EXE");
    if (exe != NULL && exe[0] != '\0' && round == 0) {
        if (cos_bitnet_spawn_capture(exe, prompt, g_cos_chat_bitnet_buf,
                                     sizeof g_cos_chat_bitnet_buf) == 0
            && g_cos_chat_bitnet_buf[0] != '\0') {
            *out_text       = g_cos_chat_bitnet_buf;
            *out_sigma      = cos_bitnet_sigma_for_prompt_and_output(
                prompt, g_cos_chat_bitnet_buf);
            *out_cost_eur   = 0.0001;
            cos_cli_escalation_record_student(g_cos_chat_bitnet_buf,
                                              *out_sigma);
            return 0;
        }
    }

    /* Branch C — deterministic self-test stub.  Keep the 2+2 fast
     * path because a number of check-*.sh harnesses pin on the
     * exact "4" answer + low σ. */
    if (round == 0 && prompt_is_what_is_2_plus_2(prompt)) {
        *out_text     = "4";
        *out_sigma    = cos_bitnet_sigma_for_local_output("4");
        *out_cost_eur = 0.0;
        cos_cli_escalation_record_student(*out_text, *out_sigma);
        return 0;
    }
    int src = cos_cli_stub_generate(prompt, round, ctx, out_text,
                                    out_sigma, out_cost_eur);
    cos_cli_escalation_record_student(*out_text, *out_sigma);
    return src;
}

int cos_cli_stub_escalate(const char *prompt, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur,
                          uint64_t *out_bytes_sent,
                          uint64_t *out_bytes_recv) {
    (void)prompt; (void)ctx;
    *out_text       = "an escalated answer from the cloud tier.";
    *out_sigma      = 0.08f;
    *out_cost_eur   = 0.012;
    *out_bytes_sent = 1024;
    *out_bytes_recv = 4096;
    return 0;
}
