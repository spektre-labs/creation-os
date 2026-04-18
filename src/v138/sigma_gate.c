/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v138 σ-Proof — ACSL-annotated σ-gate reference implementation.
 *
 * This translation unit is the machine-checkable contract for the
 * core σ-gate predicate.  Every function carries an ACSL block that
 * Frama-C's WP plug-in ("Weakest Precondition") can discharge into
 * goals handed to alt-ergo, z3, or cvc4 — the DO-178C DAL-A path.
 *
 * v138.0 ships:
 *   * this annotated source
 *   * a pure-C annotation-shape validator (always runs, tier 1)
 *   * a Makefile hook that also runs Frama-C WP *if installed*
 *     (tier 2 — skipped gracefully on machines without the tool)
 *
 * v138.1 wires the alt-ergo proof obligations into the merge-gate
 * itself and extracts the verified lemmas into Coq for the
 * highest available verification tier.
 */
#include <stdint.h>

enum {
    COS_V138_EMIT    = 0,
    COS_V138_ABSTAIN = 1,
};

/*@ requires 0.0f <= sigma <= 1.0f;
    requires 0.0f <= tau   <= 1.0f;
    assigns  \nothing;
    behavior emit:
        assumes sigma <= tau;
        ensures \result == 0;
    behavior abstain:
        assumes sigma > tau;
        ensures \result == 1;
    complete behaviors;
    disjoint behaviors;
*/
int cos_v138_sigma_gate(float sigma, float tau) {
    return (sigma > tau) ? COS_V138_ABSTAIN : COS_V138_EMIT;
}

/*@ requires \valid_read(ch + (0 .. 7));
    requires \forall integer i; 0 <= i < 8 ==> 0.0f <= ch[i] <= 1.0f;
    assigns  \nothing;
    ensures  \result == 0 || \result == 1;
    ensures  \result == 1 <==>
               (\exists integer i; 0 <= i < 8 && ch[i] > tau);
*/
int cos_v138_sigma_gate_vec8(const float *ch, float tau) {
    int deny = 0;
    /*@ loop invariant 0 <= i <= 8;
        loop invariant deny == 0 || deny == 1;
        loop invariant deny == 1 ==>
               (\exists integer j; 0 <= j < i && ch[j] > tau);
        loop invariant deny == 0 ==>
               (\forall integer j; 0 <= j < i ==> ch[j] <= tau);
        loop assigns  i, deny;
        loop variant  8 - i;
    */
    for (int i = 0; i < 8; ++i) {
        deny |= (ch[i] > tau);
    }
    return deny;
}

/*@ requires 0.0f <= sigma_old <= 1.0f;
    requires 0.0f <= sigma_new <= 1.0f;
    requires 0.0f <= delta     <= 1.0f;
    assigns  \nothing;
    ensures  \result == 0 || \result == 1;
    ensures  \result == 1 <==>
               (sigma_new - sigma_old >= delta ||
                sigma_old - sigma_new >= delta);
*/
int cos_v138_spike_trigger(float sigma_old, float sigma_new, float delta) {
    float d = sigma_new - sigma_old;
    if (d < 0) d = -d;
    return (d >= delta) ? 1 : 0;
}
