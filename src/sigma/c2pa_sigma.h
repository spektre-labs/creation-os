/*
 * C2PA-oriented σ-credential sidecar (JSON).  Not a C2PA manifest signer.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_C2PA_SIGMA_H
#define COS_SIGMA_C2PA_SIGMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Custom assertion label (string form for JSON). */
#define COS_C2PA_SIGMA_LABEL "ai.creation-os.sigma"

typedef struct cos_c2pa_sigma_assertion {
    char version[8];

    float sigma_combined;
    float sigma_semantic;
    float sigma_logprob;
    char  action[16];
    float tau_accept;
    float tau_rethink;

    char method[64];
    char model[96];
    int  n_samples;
    float temps[3];

    uint8_t content_sha256[32];
    uint8_t receipt_hash[32];
    uint8_t prev_receipt_hash[32];
    uint8_t codex_sha256[32];

    int64_t timestamp_ms;
} cos_c2pa_sigma_assertion_t;

/** Serialize assertion to a single JSON object (UTF-8, bounded). */
int cos_c2pa_sigma_assertion_to_json(const cos_c2pa_sigma_assertion_t *a,
                                     char *out, size_t cap);

/** CBOR encoding is not implemented (returns -1). */
int cos_c2pa_serialize(const cos_c2pa_sigma_assertion_t *a, uint8_t *out,
                       int max_len);

/** Write JSON sidecar for path `sidecar_path` (UTF-8). */
int cos_c2pa_sigma_write_sidecar(const char *sidecar_path,
                                 const cos_c2pa_sigma_assertion_t *a);

/** Validate a sidecar file (schema + numeric ranges + hex fields). */
int cos_c2pa_sigma_validate_file(const char *sidecar_path, FILE *report);

int cos_c2pa_sigma_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_C2PA_SIGMA_H */
