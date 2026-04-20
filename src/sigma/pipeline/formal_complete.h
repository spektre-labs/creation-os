/*
 * σ-formal-complete — honest status reporter for v259's Lean 4 +
 * Frama-C obligations.
 *
 * The kernel does NOT prove anything on its own — it parses the
 * existing artefacts and reports what's actually discharged.  That
 * way the public claim in `include/cos_version.h`
 * (`COS_FORMAL_PROOFS / COS_FORMAL_PROOFS_TOTAL`) always tracks the
 * ground truth and the CI script refuses to merge a bump that
 * isn't backed by a real artefact.
 *
 * For each v259 theorem we classify:
 *   - discharged_concrete  : Float-specific proof with no `sorry`
 *   - discharged_abstract  : LinearOrder-lifted proof (still a real
 *                            machine-checked theorem, but modulo a
 *                            NaN hypothesis; counted as partial)
 *   - pending              : carries `sorry`, not yet discharged
 *
 * ACSL side: counts `requires` / `ensures` contracts in
 * sigma_measurement.h.acsl so "n Wp obligations stated" stays in
 * sync with the theorem count.  Whether Frama-C Wp has discharged
 * them is an artefact-presence question (a `.wp/` directory); we
 * check for that without invoking the tool.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_FORMAL_COMPLETE_H
#define COS_SIGMA_FORMAL_COMPLETE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_FORMAL_MAX_THEOREMS    32
#define COS_FORMAL_NAME_MAX        64

enum cos_formal_kind {
    COS_FORMAL_DISCHARGED_CONCRETE = 0,
    COS_FORMAL_DISCHARGED_ABSTRACT = 1,
    COS_FORMAL_PENDING             = 2,
};

typedef struct {
    char name[COS_FORMAL_NAME_MAX];
    int  kind;            /* cos_formal_kind */
    int  line;            /* line number in the .lean file */
    int  has_sorry;       /* 1 if body contains `sorry` */
    int  abstract_variant;/* 1 if name ends with 'α' / .gateα */
} cos_formal_theorem_t;

typedef struct {
    cos_formal_theorem_t theorems[COS_FORMAL_MAX_THEOREMS];
    int n_theorems;
    int discharged_concrete;
    int discharged_abstract;
    int pending;

    /* ACSL */
    int acsl_requires;
    int acsl_ensures;
    int acsl_file_bytes;

    /* Header / ledger cross-check */
    int header_proofs;        /* COS_FORMAL_PROOFS */
    int header_proofs_total;  /* COS_FORMAL_PROOFS_TOTAL */
    int ledger_matches_truth; /* 1 iff header_proofs == effective discharged */
    int effective_discharged; /* the number the banner may claim */
} cos_formal_report_t;

/* Scan the two artefact files (paths supplied so tests can use
 * fixtures) and populate `report`.  Returns 0 on success, -1 on
 * missing-files, -2 on parse overflow. */
int cos_formal_complete_scan(const char *lean_path,
                             const char *acsl_path,
                             const char *version_header_path,
                             cos_formal_report_t *report);

int cos_formal_complete_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_FORMAL_COMPLETE_H */
