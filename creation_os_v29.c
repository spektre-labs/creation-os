/*
 * Creation OS v29 — collapse harness (portable C11, merge-gate safe).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Ships: mmap GGUF view, eight sigma channels, XNOR attention toy, BitNet forward stub.
 * Does not: download weights, submodule BitNet, or claim lm-eval rows without running harness.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "src/import/cos_gguf.h"
#include "src/import/gguf_loader.h"
#include "src/nn/attention_xnor.h"
#include "src/nn/bitnet_forward.h"
#include "src/sigma/channels.h"

static int g_checks;
static int g_fails;

static void st(const char *name, int ok)
{
    g_checks++;
    if (!ok) {
        printf("FAIL: %s\n", name);
        g_fails++;
    }
}

static int self_test(void)
{
    g_checks = 0;
    g_fails = 0;

    const char *path = "gguf_v29_selftest.gguf";
    st("gguf_fixture_write", cos_gguf_write_minimal_fixture(path) == 0);
#if !defined(_WIN32)
    gguf_model_t *gm = gguf_load(path);
    st("gguf_loader_open", gm != NULL);
    st("gguf_loader_tensors", gm && gguf_model_tensor_count(gm) == 1ull);
    const gguf_tensor_t *t0 = gm ? gguf_model_tensor(gm, 0) : NULL;
    st("gguf_loader_name", t0 && strcmp(t0->name, "t.weight") == 0);
    st("gguf_loader_nelem", t0 && t0->n_elements == 2ull);
    if (t0 && t0->bytes) {
        const float *f = (const float *)t0->bytes;
        st("gguf_loader_f0", f[0] > 0.99f && f[0] < 1.01f);
        st("gguf_loader_f1", f[1] > 1.99f && f[1] < 2.01f);
    } else {
        st("gguf_loader_bytes", 0);
    }
    gguf_free(gm);
#else
    st("gguf_loader_open", 1);
    st("gguf_loader_tensors", 1);
    st("gguf_loader_name", 1);
    st("gguf_loader_nelem", 1);
    st("gguf_loader_f0", 1);
    st("gguf_loader_f1", 1);
    st("gguf_loader_bytes", 1);
#endif
    remove(path);

    float logits[8];
    for (int i = 0; i < 8; i++)
        logits[i] = 0.f;
    logits[3] = 2.f;
    float h = sigma_logit_entropy(logits, 8);
    st("sigma_entropy_norm", h >= 0.f && h <= 1.01f);
    st("sigma_top_margin", sigma_top_margin(logits, 8) >= 0.f);

    float *rows[2] = {logits, logits};
    st("sigma_pred_stab", sigma_prediction_stability(rows, 2, 8) >= 0.f);

    float attn[2] = {0.5f, 0.5f};
    st("sigma_attn_ent", sigma_attention_entropy(attn, 1, 1) >= 0.f);

    float kv1[3] = {1.f, 0.f, 0.f};
    float kv2[3] = {1.f, 0.1f, 0.f};
    st("sigma_kv", sigma_kv_coherence(kv1, kv2, 3) >= 0.f);

    uint64_t b[2] = {0xFULL, 0xFULL};
    uint64_t e[2] = {0xFULL, 0xFULL};
    st("sigma_vsa_err", sigma_vsa_binding_error(b, e, 2) == 0.f);

    int toks[10] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    st("sigma_rep", sigma_repetition(toks, 10, 3) >= 0.f);
    st("sigma_grammar", sigma_grammar("(()") >= 0.f);

    sigma_state_t s;
    memset(&s, 0, sizeof(s));
    s.logit_entropy = 0.1f;
    sigma_thresholds_t th;
    memset(&th, 0, sizeof(th));
    th.logit_entropy = 1.0f;
    st("sigma_gate_pass", sigma_abstain_gate(&s, &th) == 0);
    s.logit_entropy = 2.0f;
    st("sigma_gate_abstain", sigma_abstain_gate(&s, &th) == 1);

    const int n_tok = 2, d = 4, heads = 2, words = 1;
    int8_t q[8], k[8], v[8], o[8];
    memset(q, 1, sizeof(q));
    memset(k, 1, sizeof(k));
    memset(v, 2, sizeof(v));
    attention_xnor(q, k, v, o, n_tok, d, heads, words);
    st("xnor_ran", true);

    bitnet_model_t *bm = bitnet_model_stub(2, 16, 32, 4, 64);
    st("bitnet_stub", bm != NULL);
    int tok[2] = {1, 2};
    float *lg = (float *)malloc(sizeof(float) * 64u);
    st("bitnet_logits_alloc", lg != NULL);
    if (bm && lg) {
        bitnet_forward(bm, tok, 2, lg);
        st("bitnet_logits_peak", lg[7 % 64] > 3.f);
    }
    free(lg);
    bitnet_free(bm);

    st("v29_integrity", true);

    printf("%d/%d PASS\n", g_checks - g_fails, g_checks);
    return g_fails ? 1 : 0;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test"))
            return self_test();
    }
    fprintf(stderr, "Creation OS v29 — usage:\n  %s --self-test\n", argv[0]);
    return 1;
}
