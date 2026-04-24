/*
 * Runtime proof receipts — SHA-256 bound audit trail per chat output.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PROOF_RECEIPT_H
#define COS_SIGMA_PROOF_RECEIPT_H

#include "error_attribution.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cos_error_source cos_error_source_t;

struct cos_proof_receipt {
    uint8_t codex_hash[32];
    uint8_t license_hash[32];
    uint8_t output_hash[32];
    uint8_t prev_receipt_hash[32];

    float sigma_combined;
    float sigma_channels[4];
    float meta_channels[4];
    int   gate_decision;
    cos_error_source_t attribution;

    int injection_detected;
    int risk_level;
    int sovereign_brake;
    int within_compute_budget;

    uint64_t kernel_pass_bitmap;
    int      kernels_run;
    int      kernels_passed;

    char    model_id[64];
    char    codex_version[32];
    int64_t timestamp_ms;

    int constitutional_valid;
    int constitutional_compliant;
    int constitutional_checks;
    int constitutional_violations;
    int constitutional_mandatory_halts;

    uint8_t receipt_hash[32];
};

struct cos_proof_receipt_options {
    uint64_t codex_fnv64;
    int      injection_detected;
    int      risk_level;
    int      sovereign_brake;
    int      within_compute_budget;
    int      kernels_run;
    const char *model_id;
    const char *codex_version;
    int64_t timestamp_ms;

    int constitutional_valid;
    int constitutional_compliant;
    int constitutional_checks;
    int constitutional_violations;
    int constitutional_mandatory_halts;
};

int cos_proof_receipt_generate(
    const char *output_text,
    float sigma_combined,
    const float sigma_channels[4],
    const float meta_channels[4],
    int gate_decision,
    const struct cos_error_attribution *attr,
    uint64_t kernel_bitmap,
    struct cos_proof_receipt *receipt);

int cos_proof_receipt_generate_with(
    const char *output_text,
    float sigma_combined,
    const float sigma_channels[4],
    const float meta_channels[4],
    int gate_decision,
    const struct cos_error_attribution *attr,
    uint64_t kernel_bitmap,
    const struct cos_proof_receipt_options *opts,
    struct cos_proof_receipt *receipt);

int cos_proof_receipt_verify(const struct cos_proof_receipt *receipt);

char *cos_proof_receipt_to_json(const struct cos_proof_receipt *receipt);

int cos_proof_receipt_persist(const struct cos_proof_receipt *receipt,
                              const char *path);

/** Default dir: ~/.cos/receipts ; updates chain head file. */
int cos_proof_receipt_persist_chain(struct cos_proof_receipt *receipt);

int cos_proof_receipt_chain_verify_dir(const char *dir);

void cos_proof_receipt_print_filenames(FILE *fp, const char *dir);

char *cos_proof_receipt_chain_export_jsonl(const char *dir);

int cos_proof_receipt_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_PROOF_RECEIPT_H */
