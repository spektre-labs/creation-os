/*
 * v259 — sigma_measurement_t canonical primitive.
 *
 *   This kernel fills the v258→v260 gap with the one
 *   struct every later kernel already references by
 *   string in its coupling manifest (see v290 dougong
 *   coupling rows: `channel == "sigma_measurement_t"`).
 *   Until v259 existed, that string named nothing —
 *   only an implicit contract.  v259 makes the struct
 *   concrete: exactly 12 bytes, natural alignment 4,
 *   deterministic encode/decode, deterministic gate
 *   predicate, and a measured ns/call cost on the host.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Layout (exactly 3 canonical fields in canonical
 *   order):
 *     header  uint32   4 bytes at offset 0;
 *     sigma   float    4 bytes at offset 4;
 *     tau     float    4 bytes at offset 8.
 *     Contract: `sizeof(cos_sigma_measurement_t) == 12`,
 *     `alignof(cos_sigma_measurement_t) == 4`; every
 *     offset matches the table exactly; the struct is
 *     POD-copyable by `memcpy`.
 *
 *   Roundtrip (exactly 4 canonical (σ, τ) pairs):
 *     (0.00, 0.50) allow;
 *     (0.20, 0.50) allow;
 *     (0.50, 0.50) gate_boundary (allow);
 *     (0.80, 0.50) abstain.
 *     Contract: for each pair, serialise to a 12-byte
 *     buffer via `memcpy` and deserialise back — the
 *     two structs compare bit-identical on all 12
 *     bytes; `cos_sigma_measurement_gate()` reproduces
 *     the expected verdict.
 *
 *   Gate predicate (exactly 3 canonical regimes):
 *     `signal_dominant`  σ < τ       → allow   (gate=0);
 *     `critical`         σ == τ      → allow   (gate=1);
 *     `noise_dominant`   σ > τ       → abstain (gate=2).
 *     Contract: 3 rows in canonical order; the three
 *     verdicts form a total order with no overlap; the
 *     predicate is pure (no global state) and therefore
 *     trivially thread-safe.
 *
 *   Microbench (exactly 3 canonical workloads):
 *     `encode_decode`  round-trip through memcpy;
 *     `gate_allow`     σ = 0.20  τ = 0.50 (cold branch);
 *     `gate_abstain`   σ = 0.80  τ = 0.50 (hot branch).
 *     Contract: 3 rows in canonical order; on every
 *     row `iters ≥ 1e6`, `ns_per_call < 1e6` (i.e. the
 *     hot path must cost below 1 ms/call — far
 *     looser than the actual cost on current hardware);
 *     median is strictly less than mean so the
 *     distribution has been measured, not faked.  Exact
 *     numbers vary per platform and are emitted to JSON.
 *
 *   σ_measurement (surface hygiene):
 *     σ_measurement = 1 − (
 *         layout_rows_ok + layout_size_ok +
 *           layout_align_ok +
 *         rt_rows_ok + rt_roundtrip_ok +
 *           rt_gate_verdict_ok +
 *         gate_rows_ok + gate_order_ok + gate_pure_ok +
 *         bench_rows_ok + bench_budget_ok +
 *           bench_distribution_ok
 *     ) / (3 + 1 + 1 + 4 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_measurement == 0.0`.
 *
 *   Contracts (v0):
 *     1. Layout matches the 3-row table exactly; size
 *        is 12; alignment is 4.
 *     2. Roundtrip byte-identical on all 4 canonical
 *        pairs; gate verdict matches on all 4.
 *     3. 3 gate regimes in canonical order; total
 *        order on verdict; predicate is stateless.
 *     4. 3 benches all complete ≥ 1e6 iterations,
 *        stay under the 1 ms/call budget, and report
 *        both mean and median so median-vs-mean can
 *        be inspected.
 *     5. σ_measurement ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v259.1 (named, not implemented): Frama-C ACSL
 *     annotations on `cos_sigma_measurement_gate` and
 *     `cos_sigma_measurement_roundtrip` proving purity
 *     and bit-exact roundtrip; a Lean 4 sibling
 *     (`hw/formal/v259/Measurement.lean`) proving the
 *     3-regime gate is the unique total order on σ-τ
 *     with the "critical belongs to allow" tie-break.
 *     Until those ship, the C self-test is the only
 *     enforcement.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V259_SIGMA_MEASUREMENT_H
#define COS_V259_SIGMA_MEASUREMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V259_N_LAYOUT 3
#define COS_V259_N_RT     4
#define COS_V259_N_GATE   3
#define COS_V259_N_BENCH  3

#define COS_V259_BENCH_ITERS   1000000ULL
#define COS_V259_BUDGET_NS     1000000.0       /* 1 ms/call ceiling */

/* canonical 12-byte surface that the whole stack names by string. */
typedef struct {
    uint32_t header;   /* version:8 | flags:8 | gate:8 | reserved:8 */
    float    sigma;    /* σ ∈ [0, 1]                                */
    float    tau;      /* abstain threshold                          */
} cos_sigma_measurement_t;

/* gate verdict — total order: ALLOW < GATE_BOUNDARY < ABSTAIN. */
typedef enum {
    COS_SIGMA_GATE_ALLOW          = 0,
    COS_SIGMA_GATE_BOUNDARY       = 1,
    COS_SIGMA_GATE_ABSTAIN        = 2,
} cos_sigma_gate_t;

/* Pure predicate; no global state, no allocation, no I/O. */
static inline cos_sigma_gate_t
cos_sigma_measurement_gate(const cos_sigma_measurement_t *m) {
    if (m->sigma <  m->tau) return COS_SIGMA_GATE_ALLOW;
    if (m->sigma == m->tau) return COS_SIGMA_GATE_BOUNDARY;
    return COS_SIGMA_GATE_ABSTAIN;
}

/* v259.1-range — clamp a float into [0, 1].
 *
 * NaN / -Inf / large-negative → 0.0.
 * +Inf / large-positive        → 1.0.
 * finite x ∈ [0, 1]            → x unchanged.
 *
 * Purity: no global state, no allocation, no I/O.
 * Postcondition (verified by `cos_v259_clamp_exhaustive_check`):
 *     for every IEEE-754 float x (including NaN, ±Inf, subnormals),
 *     0.0f <= cos_sigma_measurement_clamp(x) <= 1.0f.
 *
 * This is the primitive the σ-producer must call before building a
 * `cos_sigma_measurement_t` to guarantee that σ ∈ [0, 1] at the
 * struct level.  The gate predicate itself does NOT clamp — see
 * `sigma_measurement.h.acsl`, theorem 5, for why: out-of-range
 * telemetry is classifiable, not silently altered. */
static inline float
cos_sigma_measurement_clamp(float x) {
    /* NaN test: x != x is true iff x is NaN (IEEE-754). */
    if (x != x) return 0.0f;
    if (x <  0.0f) return 0.0f;
    if (x >  1.0f) return 1.0f;
    return x;
}

/* v259.1-roundtrip — named encode / decode / roundtrip helpers.
 *
 * These exist to (a) give the ACSL annotation a concrete symbol to
 * attach to, and (b) let the runtime exhaustive checker verify the
 * round-trip invariant over arbitrary bit patterns (not just the 4
 * canonical σ/τ pairs).
 *
 *   encode(m, buf)   copies the 12 bytes of `*m` into `buf`.
 *   decode(buf, m)   copies the 12 bytes of `buf` into `*m`.
 *   roundtrip(in, out)
 *                    encode(in, scratch) followed by decode(scratch, out);
 *                    postcondition: memcmp(in, out, 12) == 0.
 *
 * Purity: `memcpy` is the only side effect, restricted to the
 * explicitly named 12-byte buffer.  Thread-safe (no global state). */
static inline void
cos_sigma_measurement_encode(const cos_sigma_measurement_t *m,
                             uint8_t buf[12]) {
    __builtin_memcpy(buf, m, 12);
}

static inline void
cos_sigma_measurement_decode(const uint8_t buf[12],
                             cos_sigma_measurement_t *m) {
    __builtin_memcpy(m, buf, 12);
}

static inline bool
cos_sigma_measurement_roundtrip(const cos_sigma_measurement_t *in,
                                cos_sigma_measurement_t *out) {
    uint8_t buf[12];
    cos_sigma_measurement_encode(in, buf);
    cos_sigma_measurement_decode(buf, out);
    /* Byte-identical comparison over the full 12-byte surface.
     * We do NOT use (in == out) struct comparison because the struct
     * may carry NaN floats, and NaN != NaN in IEEE-754; the raw byte
     * comparison is the correct roundtrip invariant. */
    const uint8_t *a = (const uint8_t *)in;
    const uint8_t *b = (const uint8_t *)out;
    for (int i = 0; i < 12; ++i) if (a[i] != b[i]) return false;
    return true;
}

/* --------- v0 manifest types ----------------------------------------- */

typedef struct {
    char     field[8];
    uint32_t offset;
    uint32_t size;
} cos_v259_layout_row_t;

typedef struct {
    float            sigma;
    float            tau;
    cos_sigma_gate_t expected;
    bool             roundtrip_ok;
    cos_sigma_gate_t observed;
} cos_v259_rt_row_t;

typedef struct {
    char             label[18];
    float            sigma;
    float            tau;
    cos_sigma_gate_t verdict;
} cos_v259_gate_row_t;

typedef struct {
    char     label[18];
    uint64_t iters;
    double   mean_ns;
    double   median_ns;
    double   min_ns;
    double   max_ns;
    bool     under_budget;
} cos_v259_bench_row_t;

typedef struct {
    cos_v259_layout_row_t layout[COS_V259_N_LAYOUT];
    cos_v259_rt_row_t     rt    [COS_V259_N_RT];
    cos_v259_gate_row_t   gate  [COS_V259_N_GATE];
    cos_v259_bench_row_t  bench [COS_V259_N_BENCH];

    size_t sizeof_measurement;
    size_t alignof_measurement;

    int  n_layout_rows_ok;
    bool layout_size_ok;
    bool layout_align_ok;

    int  n_rt_rows_ok;
    bool rt_roundtrip_ok;
    bool rt_gate_verdict_ok;

    int  n_gate_rows_ok;
    bool gate_order_ok;
    bool gate_pure_ok;

    int  n_bench_rows_ok;
    bool bench_budget_ok;
    bool bench_distribution_ok;

    float sigma_measurement;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v259_state_t;

void   cos_v259_init(cos_v259_state_t *s, uint64_t seed);
void   cos_v259_run (cos_v259_state_t *s);
size_t cos_v259_to_json(const cos_v259_state_t *s, char *buf, size_t cap);
int    cos_v259_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V259_SIGMA_MEASUREMENT_H */
