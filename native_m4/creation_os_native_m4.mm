/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Native M4 lab binary (opt-in): heterogeneous dispatch helpers.
 *
 * Scope: local-only demo helpers for Apple Silicon.
 * Non-goals: merge-gate, weight downloads, end-to-end LLM claims.
 */
#include "creation_os_native_m4.h"

#include <dispatch/dispatch.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

void cos_living_weights_inplace(float *logits, const uint8_t *reputation, int vocab, float scale)
{
    if (!logits || !reputation || vocab <= 0)
        return;
    for (int i = 0; i < vocab; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
}

bool cos_runtime_has_sme(void)
{
#if defined(__APPLE__)
    int v = 0;
    size_t sz = sizeof(v);
    if (sysctlbyname("hw.optional.arm.FEAT_SME", &v, &sz, NULL, 0) == 0)
        return v != 0;
    return false;
#else
    return false;
#endif
}

static int self_test(void)
{
    /* Deterministic sanity: popcount mapping. */
    float logits[8];
    uint8_t rep[8];
    for (int i = 0; i < 8; i++) {
        logits[i] = 0.0f;
        rep[i] = (uint8_t)(1u << i); /* popcount=1 */
    }
    cos_living_weights_inplace(logits, rep, 8, 0.5f);
    /* pc=1 => (1-4)*0.5 = -1.5 */
    for (int i = 0; i < 8; i++) {
        if (logits[i] > -1.49f || logits[i] < -1.51f) {
            fprintf(stderr, "FAIL living_weights[%d]=%f expected -1.5\n", i, logits[i]);
            return 2;
        }
    }

    fprintf(stderr, "native-m4: SME runtime probe: %s\n", cos_runtime_has_sme() ? "yes" : "no");
    fprintf(stderr, "native-m4: 1/1 PASS\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && (!strcmp(argv[1], "--self-test") || !strcmp(argv[1], "--selftest")))
        return self_test();
    if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("usage: %s [--self-test]\n", argv[0]);
        printf("Native M4 lab binary (opt-in). Not part of merge-gate.\n");
        return 0;
    }

    /* Demo: run living-weights on a perf queue. */
    const int vocab = 1024;
    float *logits = (float *)aligned_alloc(64, (size_t)vocab * sizeof(float));
    uint8_t *rep = (uint8_t *)aligned_alloc(64, (size_t)vocab);
    if (!logits || !rep) {
        fprintf(stderr, "alloc failed\n");
        free(logits);
        free(rep);
        return 2;
    }
    for (int i = 0; i < vocab; i++) {
        logits[i] = 0.0f;
        rep[i] = (uint8_t)(i & 0xFF);
    }

    dispatch_queue_t q = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    dispatch_sync(q, ^{
      cos_living_weights_inplace(logits, rep, vocab, 0.125f);
    });

    printf("native-m4: logits[0]=%.3f logits[1]=%.3f (demo)\n", logits[0], logits[1]);
    free(logits);
    free(rep);
    return 0;
}

