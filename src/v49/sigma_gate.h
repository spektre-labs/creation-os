/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v49: Minimal fail-closed abstention gate on a calibrated epistemic scalar.
 *
 * This isolates the “decision” fragment for traceability / MC/DC education.
 * Full serving stacks still need v44/v48 policy wiring.
 */
#ifndef CREATION_OS_V49_SIGMA_GATE_H
#define CREATION_OS_V49_SIGMA_GATE_H

typedef enum {
    V49_ACTION_EMIT = 0,
    V49_ACTION_ABSTAIN = 1,
} v49_action_t;

/** Fail-closed: abstain if inputs are non-finite or epistemic exceeds threshold. */
v49_action_t v49_sigma_gate_calibrated_epistemic(float calibrated_epistemic, float threshold);

#endif /* CREATION_OS_V49_SIGMA_GATE_H */
