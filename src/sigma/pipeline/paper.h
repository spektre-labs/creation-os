/*
 * σ-Paper — deterministic arXiv-style paper generator.
 *
 * H5 freezes the Creation OS σ-gate thesis into one reproducible
 * manuscript.  The goal is not a hype paper.  It is a pinned
 * write-up that cites the exact numbers the C binaries on this
 * commit emit — the formal ledger (H4), the substrate
 * equivalence (H3), the photonic / spike mappings (H1, H2), and
 * the pipeline benchmarks referenced in docs/.
 *
 * Design:
 *
 *   1. The paper content is statically built from a single
 *      cos_sigma_paper_spec_t.  Numbers come from the runtime
 *      harnesses so the paper cannot drift from the code — if
 *      the formal ledger regresses, the paper prints the
 *      regression.
 *   2. The generator emits Markdown (GitHub + Pandoc-friendly).
 *      A Pandoc wrapper is out of scope for this commit.
 *   3. Output is deterministic: no clocks, no RNG, no paths.
 *      That makes the smoke test possible.
 *
 * Sections (fixed order):
 *   abstract, introduction, background, formal_results,
 *   substrate_equivalence, empirical_results, limitations,
 *   reproduction, references.
 *
 * Contract:
 *   cos_sigma_paper_generate() fills a caller buffer with the
 *   rendered Markdown.  Return value is the byte length written
 *   (not including a NUL terminator, though one is written if
 *   the buffer has room).  Negative values are errors.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PAPER_H
#define COS_SIGMA_PAPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_PAPER_MAX_BYTES  (128 * 1024)

typedef struct {
    /* Formal ledger — H4.                                        */
    uint32_t t3_witnesses;
    uint32_t t4_witnesses;
    uint32_t t5_witnesses;
    uint32_t t6_witnesses;
    uint64_t t6_median_ns;
    uint64_t t6_p99_ns;
    uint64_t t6_bound_ns;
    int      all_discharged;   /* 4/4 if nonzero                 */

    /* Substrate equivalence — H3.  Pinned σ for the canonical
     * "dominant" activation across the four backends.           */
    float    sigma_digital;
    float    sigma_bitnet;
    float    sigma_spike;
    float    sigma_photonic;
    int      substrates_equivalent;

    /* Benchmark headline — reused from the pipeline harness
     * (cos benchmark).  We do not rerun benchmarks here; values
     * are supplied by the caller so the paper's numbers are
     * explicit inputs.                                          */
    int      benchmark_fixtures;
    float    pipeline_savings_percent;     /* vs api_only        */
    float    hybrid_monthly_eur;
    float    cloud_monthly_eur;
    float    local_fraction_percent;
} cos_sigma_paper_spec_t;

/* Populate spec with the canonical committed values.  No I/O.  */
void cos_sigma_paper_default_spec(cos_sigma_paper_spec_t *out);

/* Render the paper into `buf`.  Returns bytes written on success
 * (excluding trailing NUL), -1 on nil args, -2 on capacity. */
int  cos_sigma_paper_generate(const cos_sigma_paper_spec_t *spec,
                              char *buf, size_t cap);

/* Section count and total byte length for a rendered paper.   */
int  cos_sigma_paper_section_count(void);

int  cos_sigma_paper_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_PAPER_H */
