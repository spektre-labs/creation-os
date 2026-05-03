/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Footprint notes for `sigma_gate_tiny.h` on constrained MCUs.
 *
 * **Evidence:** structure sizes are fixed by `int32_t` × 3. ROM/RAM totals for
 * the **full firmware** depend on the toolchain, libc, and drivers — treat
 * comparative TFLM / vendor numbers below as **order-of-magnitude**, not merge-gate
 * measurements (see docs/CLAIM_DISCIPLINE.md).
 */

#ifndef SIGMA_FOOTPRINT_H
#define SIGMA_FOOTPRINT_H

/*
 * sigma_state_t: 3 × int32_t → 12 bytes on normal 32-bit MCU ABI.
 *
 * Gate path: sigma_update + sigma_gate — no heap, no recursion.
 * Typical gate machine code: **hundreds of bytes** ROM (not audited per ISA).
 *
 * σ-sensor wrapper (sigma_sensor_t): +12 bytes state + counters ≈ **28 bytes**
 * RAM if uint32_t anomalies included.
 */

#define SIGMA_TINY_STATE_RAM_BYTES   12

/*
 * Rough ecosystem comparisons (marketing datasheets / typical demos, not this repo):
 * - Frameworks like TFLM / Edge Impulse often quote **tens of kiB** RAM+flash
 *   for even small INT8 graphs — σ-gate is a **policy primitive**, not a substitute
 *   for a full neural graph.
 */

#endif /* SIGMA_FOOTPRINT_H */
