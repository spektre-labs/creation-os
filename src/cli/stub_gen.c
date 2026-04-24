/* Reference stub generator — see stub_gen.h. */

#include "stub_gen.h"

#include "bitnet_server.h"
#include "bitnet_spawn.h"
#include "speculative_decode.h"
#include "bitnet_sigma.h"
#include "codex.h"
#include "cos_tui.h"
#include "cost_log.h"
#include "escalation.h"
#include "engram.h"
#include "ttt_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Matches bitnet_server.c BNS_TEXT_CAP — max_tokens must not exceed what we
 * can copy back from llama-server (same order as reasoning-heavy replies). */
#define COS_CLI_BITNET_TEXT_CAP (512u * 1024u)
static char g_cos_chat_bitnet_buf[COS_CLI_BITNET_TEXT_CAP];
static char g_cos_ttt_augment_buf[COS_CLI_BITNET_TEXT_CAP];

/* AGI-1: ICL-wrapped prompt (few-shot prefix + final user line). */
static char g_cos_chat_icl_buf[16384];

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

/* Use OpenAI-compat HTTP when either flag is set: COS_BITNET_SERVER alone
 * was easy to omit when COS_BITNET_SERVER_EXTERNAL=1 already pins a remote
 * llama-server — falling through to cos_bitnet_spawn_capture() passes -p,
 * which llama-server rejects (that flag is for llama-cli). */
int cos_cli_use_bitnet_http(void) {
    const char *s = getenv("COS_BITNET_SERVER");
    if (s != NULL && s[0] == '1') return 1;
    const char *e = getenv("COS_BITNET_SERVER_EXTERNAL");
    return (e != NULL && e[0] == '1');
}

/* Default was 64 max_tokens → answers looked like cut-off reasoning.
 * COS_BITNET_CHAT_MAX_TOKENS overrides; else COS_BITNET_SERVER_NPRED;
 * else 4096.  Clamped [32, 8192]. */
static int cos_cli_bitnet_chat_max_tokens(void) {
    const char *e = getenv("COS_BITNET_CHAT_MAX_TOKENS");
    int          n = 0;
    if (e != NULL && e[0] != '\0')
        n = atoi(e);
    if (n <= 0 && (e = getenv("COS_BITNET_SERVER_NPRED")) != NULL && e[0] != '\0')
        n = atoi(e);
    if (n <= 0)
        n = 4096;
    if (n < 32)
        n = 32;
    if (n > 8192)
        n = 8192;
    return n;
}

static int cos_chat_stream_cb(const char *tok, float sigma,
                              int is_last, void *ctx) {
    cos_chat_stream_ctx_t *s = (cos_chat_stream_ctx_t *)ctx;
    const char       *tui_env = getenv("COS_CHAT_TUI");
    int               use_tui = (tui_env != NULL && tui_env[0] == '1');

    if (tok == NULL || tok[0] == '\0') {
        /* Terminal chunk (stop=true) — server emits empty content. */
        if (is_last && s->have_printed) {
            if (use_tui)
                cos_tui_stream_newline();
            else {
                fputc('\n', stdout);
                fflush(stdout);
            }
        }
        return 0;
    }
    if (use_tui)
        cos_tui_print_token(tok);
    else
        fputs(tok, stdout);
    if (!use_tui && s->verbose)
        fprintf(stdout, "[σ=%.2f]", (double)sigma);
    fflush(stdout);
    s->have_printed = 1;
    return 0;
}

/* Backend selection (DEV-1 / DEV-4):
 *
 *   COS_BITNET_SERVER=1 or
 *   COS_BITNET_SERVER_EXTERNAL=1 → HTTP POST to llama-server (bitnet_server.h)
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
    /* DEV-6 / AGI-1: ctx is either a legacy `cos_sigma_codex_t *` or a
     * `cos_cli_generate_ctx_t *` tagged with COS_CLI_GENERATE_CTX_MAGIC.
     * The latter enables SQLite-backed few-shot ICL before local model
     * calls (BitNet / llama-server); keyword stubs still key off the
     * raw user `prompt` so harnesses stay stable. */
    const cos_sigma_codex_t *codex = NULL;
    cos_cli_generate_ctx_t   *wrap = NULL;
    if (ctx != NULL) {
        uint32_t m = *(const uint32_t *)ctx;
        if (m == COS_CLI_GENERATE_CTX_MAGIC) {
            wrap  = (cos_cli_generate_ctx_t *)ctx;
            codex = wrap->codex;
        } else {
            codex = (const cos_sigma_codex_t *)ctx;
        }
    }
    const char *codex_sysprompt =
        (codex != NULL && codex->bytes != NULL && codex->size > 0)
            ? codex->bytes : NULL;
    if (prompt == NULL) {
        *out_text = "";
        *out_sigma = 1.0f;
        *out_cost_eur = 0.0;
        return 1;
    }

    const char *prompt_model = prompt;
    if (wrap != NULL && wrap->icl_compose != NULL && wrap->icl_ctx != NULL
        && wrap->icl_k > 0
        && (!wrap->icl_rethink_only || round > 0)) {
        uint64_t h = cos_sigma_engram_hash(prompt);
        if (wrap->icl_compose(wrap->icl_ctx, h,
                              wrap->icl_exemplar_max_sigma,
                              wrap->icl_k, prompt,
                              g_cos_chat_icl_buf,
                              sizeof g_cos_chat_icl_buf) == 0) {
            prompt_model = g_cos_chat_icl_buf;
        }
    }

    /* Branch A — llama-server backend (real per-token σ). */
    if (cos_cli_use_bitnet_http()) {
        (void)unsetenv("COS_SPEC_DECODE_ROUTE");

        const char *prompt_send = prompt_model;
        const char *ttt_env     = getenv("COS_CHAT_TTT");
        if (ttt_env != NULL && ttt_env[0] == '1' && round > 0) {
            size_t nw = cos_ttt_augment_prompt(g_cos_ttt_augment_buf,
                                               sizeof(g_cos_ttt_augment_buf),
                                               prompt_model);
            if (nw > 0)
                prompt_send = g_cos_ttt_augment_buf;
        }

        const char *specdec = getenv("COS_SPEC_DECODE");
        if (specdec != NULL && specdec[0] == '1' && round == 0) {
            cos_spec_decode_result_t sr;
            memset(&sr, 0, sizeof(sr));
            cos_spec_decode_init(NULL);
            if (cos_spec_decode_run(prompt_send, codex_sysprompt, &sr)
                == 0) {
                size_t n = strlen(sr.response);
                if (n >= sizeof(g_cos_chat_bitnet_buf))
                    n = sizeof(g_cos_chat_bitnet_buf) - 1;
                memcpy(g_cos_chat_bitnet_buf, sr.response, n);
                g_cos_chat_bitnet_buf[n] = '\0';
                *out_text     = g_cos_chat_bitnet_buf;
                *out_sigma    = sr.sigma;
                *out_cost_eur = 0.0001;
                setenv("COS_SPEC_DECODE_ROUTE",
                       sr.used_draft ? "DRAFT" : "VERIFY", 1);
                cos_cli_escalation_record_student(g_cos_chat_bitnet_buf,
                                                  sr.sigma);
                cos_cost_log_append(sr.used_draft ? "spec_draft"
                                                  : "spec_verify",
                                    "LOCAL", 0,
                                    (int)(n / 4u + 1u),
                                    sr.sigma, 0.0001);
                return 0;
            }
            /* Failed — fall through to single-model path. */
        }
        cos_bitnet_server_params_t p;
        memset(&p, 0, sizeof(p));
        p.n_predict   = cos_cli_bitnet_chat_max_tokens();
        /* Ollama + OpenAI-compat peers: keep top_logprobs small (latency). */
        p.n_probs     = 3;
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
            rc = cos_bitnet_server_complete_stream(prompt_send, &p,
                                                   cos_chat_stream_cb,
                                                   &sctx, &r);
        } else {
            rc = cos_bitnet_server_complete(prompt_send, &p, &r);
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
            /* POLISH-4: record the successful LOCAL call into the
             * per-inference cost log.  tokens_in is unknown from the
             * backend shape so we pass 0; the pipeline tracks it at
             * the outer layer for higher-level reporting. */
            cos_cost_log_append("bitnet", "LOCAL",
                                0, r.token_count,
                                r.sigma, r.cost_eur);
            return 0;
        }
        /* Server failed → fall through to legacy path.  This keeps
         * the CLI usable when the model file is missing or the
         * server crashed mid-session. */
    }

    /* Branch B — legacy subprocess capture with heuristic σ.
     * Never mix this with COS_BITNET_SERVER_EXTERNAL: the exe is often
     * llama-server, which does not accept -p (see cos_cli_use_bitnet_http). */
    const char *ext_spawn_block = getenv("COS_BITNET_SERVER_EXTERNAL");
    const char *exe = getenv("CREATION_OS_BITNET_EXE");
    if ((ext_spawn_block == NULL || ext_spawn_block[0] != '1')
        && exe != NULL && exe[0] != '\0' && round == 0) {
        if (cos_bitnet_spawn_capture(exe, prompt_model, g_cos_chat_bitnet_buf,
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
        /* POLISH-4: even stub calls are real pipeline passes from the
         * user's perspective.  Log them with provider=stub so
         * `cos cost --from-log` has something to show when no server
         * or API key is configured. */
        cos_cost_log_append("stub", "LOCAL", 0, 1, *out_sigma, *out_cost_eur);
        return 0;
    }
    int src = cos_cli_stub_generate(prompt, round, ctx, out_text,
                                    out_sigma, out_cost_eur);
    cos_cli_escalation_record_student(*out_text, *out_sigma);
    cos_cost_log_append("stub", "LOCAL", 0, 1, *out_sigma, *out_cost_eur);
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
