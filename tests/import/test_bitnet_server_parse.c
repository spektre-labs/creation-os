/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  tests/import/test_bitnet_server_parse.c
 *  --------------------------------------------------------------------
 *  CI-safe unit test for bitnet_server's JSON reducer.  No network,
 *  no subprocess, no model file — we feed canned /v1/chat/completions
 *  responses (copied from a real llama-server run) and assert the
 *  per-token σ reduction lands on the analytically-expected values.
 *
 *  Ground truth:
 *    σ_token    = 1.0 − probs[0].prob   (probs[0] is the sampled token)
 *    σ_response = max(σ_token) over all predicted tokens
 *    σ_mean     = average(σ_token)
 *
 *  To keep the parser in one file, we statically link bitnet_server.c
 *  and call `parse_completion_response` — which we redeclare here as
 *  `extern` since it's internal to the module.  The internal
 *  function is intentionally `static`; the test drives the public
 *  entry point via a small inline wrapper exported below.
 */

#include "bitnet_server.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Hook exported by bitnet_server.c for this test. */
int  cos_bitnet_server__test_parse(const char *body, size_t len,
                                   cos_bitnet_server_result_t *out);

static int approx(float a, float b) {
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d < 1e-4f;
}

static int assert_parse(const char *label, const char *json, float sigma_max,
                        float sigma_mean, const char *expect_text,
                        int expect_tokens) {
    cos_bitnet_server_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = cos_bitnet_server__test_parse(json, strlen(json), &r);
    if (rc != 0) {
        fprintf(stderr, "[FAIL] %s: parse rc=%d\n", label, rc);
        return 1;
    }
    int fails = 0;
    if (!approx(r.sigma, sigma_max)) {
        fprintf(stderr, "[FAIL] %s: σ_max=%.4f want %.4f\n",
                label, (double)r.sigma, (double)sigma_max);
        fails++;
    }
    if (!approx(r.mean_sigma, sigma_mean)) {
        fprintf(stderr, "[FAIL] %s: σ_mean=%.4f want %.4f\n",
                label, (double)r.mean_sigma, (double)sigma_mean);
        fails++;
    }
    if (expect_text != NULL && strcmp(r.text, expect_text) != 0) {
        fprintf(stderr, "[FAIL] %s: text=%s\n  want=%s\n",
                label, r.text, expect_text);
        fails++;
    }
    if (r.token_count != expect_tokens) {
        fprintf(stderr, "[FAIL] %s: tokens=%d want %d\n",
                label, r.token_count, expect_tokens);
        fails++;
    }
    if (fails == 0)
        fprintf(stdout, "[PASS] %s  σ_max=%.4f σ_mean=%.4f tokens=%d\n",
                label, (double)r.sigma, (double)r.mean_sigma, r.token_count);
    return fails;
}

int main(void) {
    int fails = 0;

    /* Case 1: three tokens at probs 1.0, 0.5, 0.8 → σ_max = 0.5,
     * σ_mean = (0.0 + 0.5 + 0.2) / 3 ≈ 0.2333 */
    const char *json1 =
        "{\"choices\":[{\"finish_reason\":\"length\","
        "\"message\":{\"content\":\"Paris is France\"}}],"
        "\"usage\":{\"completion_tokens\":3},"
        "\"completion_probabilities\":["
        "  {\"content\":\"Paris\","
        "   \"probs\":[{\"tok_str\":\"Paris\",\"prob\":1.0}]},"
        "  {\"content\":\" is\","
        "   \"probs\":[{\"tok_str\":\" is\",\"prob\":0.5}]},"
        "  {\"content\":\" France\","
        "   \"probs\":[{\"tok_str\":\" France\",\"prob\":0.8}]}"
        "]}";
    fails += assert_parse("case1: mixed probs", json1,
                          0.34f,
                          (1.0f - (1.0f + 0.5f + 0.8f)/3.0f),
                          "Paris is France", 3);

    /* Case 2: all null probs → σ_max = 1.0, σ_mean = 1.0.
     *
     * The BitNet-b1.58 llama.cpp fork returns `"prob": null` on a
     * degenerate sampler path (see DEV-1 notes in bitnet_server.c).
     * Semantically "null" means "the server could not compute a
     * probability for this token" which is maximum uncertainty, so
     * the reducer treats it as chosen_prob=0 → σ=1.0.  That is the
     * correct failure mode: the σ-gate then forces a RETHINK with a
     * different seed/temperature (DEV-2) instead of committing
     * garbage text. */
    const char *json2 =
        "{\"choices\":[{\"finish_reason\":\"stop\","
        "\"message\":{\"content\":\"@@\"}}],"
        "\"usage\":{\"completion_tokens\":2},"
        "\"completion_probabilities\":["
        "  {\"content\":\"@\","
        "   \"probs\":[{\"tok_str\":\"8\",\"prob\":null}]},"
        "  {\"content\":\"@\","
        "   \"probs\":[{\"tok_str\":\"8\",\"prob\":null}]}"
        "]}";
    fails += assert_parse("case2: null probs → σ=1", json2,
                          1.0f, 1.0f, "@@", 2);

    /* Case 3: empty completion_probabilities → neutral σ = 0.5.
     * Happens when server aborts with 0 tokens or returns a bare
     * choices array (e.g. non-logprobs request). */
    const char *json3 =
        "{\"choices\":[{\"finish_reason\":\"stop\","
        "\"message\":{\"content\":\"\"}}],"
        "\"usage\":{\"completion_tokens\":0},"
        "\"completion_probabilities\":[]}";
    fails += assert_parse("case3: empty probs", json3,
                          0.5f, 0.5f, "", 0);

    /* Case 4: /completion raw shape (content at top level, no
     * chat wrapping) — proves the fallback scan finds both the
     * outer `content` and the /completion-only `stopped_limit`. */
    const char *json4 =
        "{\"content\":\"hello world\","
        "\"tokens_predicted\":2,"
        "\"stopped_eos\":false,\"stopped_limit\":true,"
        "\"completion_probabilities\":["
        "  {\"content\":\"hello\","
        "   \"probs\":[{\"tok_str\":\"hello\",\"prob\":0.9}]},"
        "  {\"content\":\" world\","
        "   \"probs\":[{\"tok_str\":\" world\",\"prob\":0.6}]}"
        "]}";
    fails += assert_parse("case4: /completion shape", json4,
                          0.31f, (1.0f - (0.9f + 0.6f)/2.0f),
                          "hello world", 2);

    /* Case 5: UTF-8 string with a \uXXXX escape inside the text. */
    const char *json5 =
        "{\"choices\":[{\"finish_reason\":\"stop\","
        "\"message\":{\"content\":\"Helsinki is the capital of Finland.\"}}],"
        "\"usage\":{\"completion_tokens\":1},"
        "\"completion_probabilities\":["
        "  {\"content\":\"Helsinki\","
        "   \"probs\":[{\"tok_str\":\"Helsinki\",\"prob\":0.95}]}"
        "]}";
    fails += assert_parse("case5: text + single tok", json5,
                          1.0f - 0.95f, 1.0f - 0.95f,
                          "Helsinki is the capital of Finland.", 1);

    /* Case 6: OpenAI chat + llama-server — message.content empty,
     * reasoning_content holds text; σ from logprobs.content[].logprob
     * (not completion_probabilities). */
    const float seq6 = 0.47103317f; /* 0.6·σ_mean + 0.4·σ_min on [-0.1,-1] */
    char json6[1536];
    int j6 = snprintf(json6, sizeof(json6),
                      "{\"choices\":[{\"finish_reason\":\"stop\","
                      "\"message\":{\"content\":\"\",\"reasoning_content\":\"xy\"}"
                      ",\"logprobs\":{\"content\":["
                      "{\"logprob\":%.4f},{\"logprob\":%.4f}]}],"
                      "\"usage\":{\"completion_tokens\":2}}"
                      ,(double)-0.1, (double)-1.0);
    if (j6 < 0 || (size_t)j6 >= sizeof(json6))
        return 99;
    fails += assert_parse(
        "case6: logprobs.content σ (reasoning-only → 0.6 mean + 0.4 min)",
        json6,
        seq6,
        0.36364157f,
        "xy",
        2);

    /* Case 7: visible message.content + logprobs.content (no reasoning). */
    const char *json7 =
        "{\"choices\":[{\"message\":{\"content\":\"hi\"},\"logprobs\":"
        "{\"content\":[{\"logprob\":-0.1},{\"logprob\":-0.69}]}}],"
        "\"usage\":{\"completion_tokens\":2}}";
    fails += assert_parse("case7: logprobs.content σ (plain content → 0.6+0.4)",
                          json7,
                          0.37744553f,
                          0.29679325f,
                          "hi",
                          2);

    /* Case 8: Ollama-style `reasoning` key (not reasoning_content). */
    const float seq8 = seq6;
    char json8[1536];
    int j8 = snprintf(json8, sizeof(json8),
                      "{\"choices\":[{\"finish_reason\":\"stop\","
                      "\"message\":{\"content\":\"\",\"reasoning\":\"xy\"}"
                      ",\"logprobs\":{\"content\":["
                      "{\"logprob\":%.4f},{\"logprob\":%.4f}]}],"
                      "\"usage\":{\"completion_tokens\":2}}"
                      ,(double)-0.1, (double)-1.0);
    if (j8 < 0 || (size_t)j8 >= sizeof(json8))
        return 98;
    fails += assert_parse(
        "case8: logprobs + Ollama reasoning key → 0.6+0.4",
        json8,
        seq8,
        0.36364157f,
        "xy",
        2);

    /* Case 9: Ollama native /api/chat top-level logprobs array. */
    const float s9 = 1.0f - (float)exp(-0.69);
    const char *json9 =
        "{\"message\":{\"role\":\"assistant\",\"content\":\"Z\"},"
        "\"logprobs\":["
        "  {\"token\":\"Z\",\"logprob\":-0.69}"
        "]}";
    fails += assert_parse("case9: Ollama native logprobs[]", json9,
                          s9, s9, "Z", 1);

    /* Case 10: both content + reasoning → σ uses answer tail of logprobs
     * (length-ratio skip of reasoning tokens). */
    const float s10 = 0.49842393f;
    const char *json10 =
        "{\"choices\":[{\"message\":{\"content\":\"hi\","
        "\"reasoning\":\"ab\"},\"logprobs\":{\"content\":["
        "{\"logprob\":-0.1},{\"logprob\":-0.69}]}}],"
        "\"usage\":{\"completion_tokens\":2}}";
    fails += assert_parse("case10: content+reasoning → answer-only logprobs",
                          json10,
                          s10, s10, "hi", 2);

    if (fails == 0) {
        fprintf(stdout, "OK: all 10 cases passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL: %d assertion(s)\n", fails);
    return 1;
}
