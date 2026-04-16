/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v47: ZK-σ proof API — architecture + honesty tier (see docs/v47/INVARIANT_CHAIN.md).
 *
 * Sound zero-knowledge verification is **not** implemented here (P-tier). The
 * structs and call shapes exist so a future circuit-backed prover can replace
 * the stub bodies without churning the rest of the stack.
 */
#ifndef CREATION_OS_V47_ZK_SIGMA_H
#define CREATION_OS_V47_ZK_SIGMA_H

#include "../sigma/decompose.h"

#include <stdint.h>

typedef struct {
    uint8_t proof[256];
    uint8_t commitment[32];
    sigma_decomposed_t sigma;
    int verified;
} zk_sigma_proof_t;

zk_sigma_proof_t prove_sigma(const float *logprobs, int n);

int verify_sigma_proof(zk_sigma_proof_t *proof);

#endif /* CREATION_OS_V47_ZK_SIGMA_H */
