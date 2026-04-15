#include "mcts_reasoner.h"

#include "facts_store.h"
#include "router.h"
#include "hdc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mcts_score_answer(const char *text, float *sigma_v, alm_attest_t *att) {
    if (!text || !text[0] || g_nfacts <= 0) {
        *sigma_v = 1.f;
        *att = ALM_HALLUCINATION;
        return;
    }
    char anorm[MAXQ];
    facts_norm_line(anorm, text, sizeof anorm);
    HV    av = hv_encode(anorm);
    int   vh = HDC_D;
    float ver_dot = -2.f;
    if (hdc_nearest_among(&av, g_hvec, g_nfacts, &vh) >= 0)
        ver_dot = hdc_sim_from_hamming(vh);
    *sigma_v = (ver_dot > -1.5f) ? fmaxf(0.f, 1.f - ver_dot) : 1.f;
    if (ver_dot >= ROUTER_VERIFY_ATTESTED_DOT)
        *att = ALM_ATTESTED;
    else if (ver_dot >= ROUTER_VERIFY_PARTIAL_DOT)
        *att = ALM_PARTIAL;
    else if (ver_dot >= ROUTER_VERIFY_UNKNOWN_DOT)
        *att = ALM_UNKNOWN;
    else
        *att = ALM_HALLUCINATION;
}

int mcts_budget_from_env(void) {
    const char *e = getenv("CREATION_OS_MCTS_BUDGET");
    if (!e || !e[0])
        return 0;
    int b = atoi(e);
    if (b < 1)
        return 0;
    if (b > 32)
        b = 32;
    return b;
}

float mcts_sigma_thr_from_env(void) {
    const char *e = getenv("CREATION_OS_MCTS_SIGMA_THR");
    if (!e || !e[0])
        return 0.25f;
    float t = (float)atof(e);
    if (t < 0.f)
        return 0.f;
    if (t > 1.f)
        return 1.f;
    return t;
}

int mcts_sigma_rollout(const char *question, int budget, float temp_base, float temp_step,
                       float sigma_early_stop, mcts_sigma_result_t *out) {
    if (!out || !question || !question[0] || budget < 1)
        return 0;
    memset(out, 0, sizeof *out);
    out->sigma = 1.f;
    out->value = -1.f;
    out->attest = ALM_HALLUCINATION;
    out->visits = 0;
    out->rollouts_done = 0;

    int any = 0;
    for (int i = 0; i < budget; i++) {
        double t = (double)temp_base + (double)i * (double)temp_step;
        if (t < 0.05)
            t = 0.05;
        if (t > 1.5)
            t = 1.5;

        char *raw = alm_llm_generate_filebased_temp(question, t);
        if (!raw)
            continue;
        any = 1;
        char trim[ALM_ANSWER_MAX];
        strncpy(trim, raw, sizeof trim - 1);
        trim[sizeof trim - 1] = 0;
        free(raw);

        float        sig;
        alm_attest_t at;
        mcts_score_answer(trim, &sig, &at);
        float val = 1.f - sig;

        int better = 0;
        if (out->value < 0.f)
            better = 1;
        else if (val > out->value + 1e-6f)
            better = 1;
        else if (fabsf(val - out->value) <= 1e-6f && sig < out->sigma - 1e-6f)
            better = 1;
        else if (fabsf(val - out->value) <= 1e-6f && fabsf(sig - out->sigma) <= 1e-6f && at < out->attest)
            better = 1;

        if (better) {
            strncpy(out->answer, trim, sizeof out->answer - 1);
            out->answer[sizeof out->answer - 1] = 0;
            out->sigma = sig;
            out->attest = at;
            out->value = val;
            out->visits = 1;
        }
        out->rollouts_done++;

        if (sig < sigma_early_stop)
            break;
    }
    return any;
}

static void dirname_of(const char *argv0, char *out, size_t cap) {
    const char *slash = strrchr(argv0, '/');
    if (!slash || slash == argv0) {
        if (cap > 2) {
            out[0] = '.';
            out[1] = 0;
        }
        return;
    }
    size_t n = (size_t)(slash - argv0);
    if (n + 1 >= cap)
        n = cap - 2;
    memcpy(out, argv0, n);
    out[n] = 0;
}

static void print_json_string(const char *s) {
    fputc('"', stdout);
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            fputc('\\', stdout);
            fputc((char)c, stdout);
        } else if (c == '\n' || c == '\r' || c == '\t')
            fputc(' ', stdout);
        else if (c < 32)
            continue;
        else
            fputc((char)c, stdout);
    }
    fputc('"', stdout);
}

int mcts_run_eval_json(const char *argv0) {
    char dir[1024];
    dirname_of(argv0, dir, sizeof dir);
    char path[1200];
    snprintf(path, sizeof path, "%s/causal_smoke_facts.json", dir);

    alm_set_llm_script_from_argv0(argv0);
    if (alm_init(path) != 0) {
        fprintf(stderr, "mcts-eval: alm_init failed: %s\n", path);
        return 1;
    }

    const char *q = getenv("CREATION_OS_MCTS_EVAL_Q");
    if (!q || !q[0])
        q = "Why does ice melt at room temperature? One sentence.";

    mcts_sigma_result_t R;
    int                 b = 5;
    const char         *be = getenv("CREATION_OS_MCTS_EVAL_BUDGET");
    if (be && be[0])
        b = atoi(be);
    if (b < 1)
        b = 1;
    if (b > 16)
        b = 16;

    int ok = mcts_sigma_rollout(q, b, 0.3f, 0.1f, 0.1f, &R);
    printf("{\"mcts_eval\":true,\"budget\":%d,\"ok\":%s,\"sigma\":%.6f,\"value\":%.6f,"
           "\"attest\":%d,\"rollouts\":%d,\"question\":",
           b, ok ? "true" : "false", (double)R.sigma, (double)R.value, (int)R.attest, R.rollouts_done);
    print_json_string(q);
    printf(",\"answer\":");
    print_json_string(R.answer);
    printf("}\n");
    return 0;
}
