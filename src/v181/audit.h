/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v181 σ-Audit — the trail of every σ-decision.
 *
 *   v160 telemetry measures throughput.
 *   v179 explains mechanism.
 *   v181 builds the audit trail that holds up to an external
 *   inspection (EU AI Act Article 13, ISO/IEC 42001, etc.).
 *
 *   Per entry:
 *
 *     id            uuid-like monotonic
 *     timestamp     ISO8601 string (deterministic in self-tests)
 *     input_hash    SHA-256(prompt)
 *     output_hash   SHA-256(response)
 *     sigma_product σ as used by the gate
 *     sigma_channels 8 per-channel σ floats
 *     decision      emit / abstain / revise
 *     explanation   one-line v179 reason
 *     steering      joined v180 vectors applied
 *     specialist    e.g. "bitnet-2b"
 *     adapter_ver   e.g. "continual_epoch_47"
 *     prev_hash     SHA-256 of previous canonical entry
 *     sig           attestation (keyed SHA-256 in v181.0;
 *                                ed25519 signature in v181.1)
 *
 *   Chain property:
 *      entry N.prev_hash == canonical_hash(entry N-1)
 *      tampering any field breaks `cos_v181_verify_chain()`.
 *
 *   Exit invariants (merge-gate):
 *     1. Self-test verifies a 1 000-entry chain end-to-end.
 *     2. Tampering any byte of a middle entry is detected.
 *     3. Tampering any sig byte is detected.
 *     4. Anomaly detection flags an injected σ-spike (≥ 30 %
 *        σ_mean rise over a rolling window).
 *     5. JSONL export reports the same number of entries as the
 *        in-memory log, with byte-for-byte equal canonical hashes.
 *
 *   v181.1 (named, not shipped):
 *     - real ed25519 signatures (libsodium);
 *     - `cos audit report --period YYYY-MM` → PDF;
 *     - `cos audit export --format jsonl` + external verifier CLI;
 *     - auto-trigger v159 heal on anomaly.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V181_AUDIT_H
#define COS_V181_AUDIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V181_MAX_ENTRIES    1024
#define COS_V181_N_CHANNELS        8
#define COS_V181_STR_MAX          96
#define COS_V181_EXPL_MAX        192

typedef enum {
    COS_V181_DECISION_EMIT     = 0,
    COS_V181_DECISION_ABSTAIN  = 1,
    COS_V181_DECISION_REVISE   = 2,
} cos_v181_decision_t;

typedef struct {
    uint64_t  id;
    char      timestamp[32];                  /* ISO8601 */
    uint8_t   input_hash [32];
    uint8_t   output_hash[32];
    float     sigma_product;
    float     sigma_channels[COS_V181_N_CHANNELS];
    cos_v181_decision_t decision;
    char      explanation[COS_V181_EXPL_MAX];
    char      steering   [COS_V181_STR_MAX];
    char      specialist [COS_V181_STR_MAX];
    char      adapter_ver[COS_V181_STR_MAX];

    uint8_t   prev_hash[32];                  /* links to N-1 */
    uint8_t   sig[32];                        /* keyed-SHA256 attest */
    uint8_t   self_hash[32];                  /* canonical hash of N */
} cos_v181_entry_t;

typedef struct {
    int     n_entries;
    cos_v181_entry_t *entries;                 /* heap array */

    /* Pinned attestation key (v181.0 keyed hash).  v181.1 swaps
     * to an ed25519 signing key stored in models/v181/sign.key. */
    uint8_t key[32];

    /* Anomaly detection window. */
    int     window;                            /* samples */
    float   anomaly_rel_rise_pct;              /* trigger, e.g. 30 */

    bool    anomaly_detected;
    int     anomaly_at;                        /* entry index, -1 if none */
    float   anomaly_sigma_baseline;            /* prior window mean */
    float   anomaly_sigma_recent;              /* latest window mean */
    float   anomaly_rel_rise;                  /* computed rise % */

    uint64_t seed;
} cos_v181_state_t;

int  cos_v181_init    (cos_v181_state_t *s, int capacity,
                       uint64_t seed);
void cos_v181_free    (cos_v181_state_t *s);

/* Append an entry, auto-populating prev_hash / self_hash / sig.
 * Returns 0 on success, non-zero if the chain is full. */
int  cos_v181_append  (cos_v181_state_t *s,
                       const char *input,   /* null-terminated */
                       const char *output,
                       float sigma_product,
                       const float channels[COS_V181_N_CHANNELS],
                       cos_v181_decision_t decision,
                       const char *explanation,
                       const char *steering,
                       const char *specialist,
                       const char *adapter_ver,
                       const char *timestamp);

/* Recompute every self_hash/sig and verify prev_hash chain.
 * Returns 0 iff chain is intact; non-zero returns the 1-based
 * index of the first bad entry. */
int  cos_v181_verify_chain(const cos_v181_state_t *s);

/* Scan for anomalies using (mean σ in last `window` entries) vs
 * (mean σ in prior window).  Sets anomaly_* fields in state.
 * Returns 1 if an anomaly was detected, 0 otherwise. */
int  cos_v181_detect_anomaly(cos_v181_state_t *s);

/* Deterministic synthetic populate for self-tests.  Fills `n`
 * entries whose σ is mostly low, then injects a σ-spike near
 * the tail to exercise anomaly detection. */
void cos_v181_populate_fixture(cos_v181_state_t *s, int n);

size_t cos_v181_to_json(const cos_v181_state_t *s,
                        char *buf, size_t cap);

/* JSONL export: one entry per line (hashes as hex, canonical
 * signature as hex).  Returns bytes written. */
size_t cos_v181_export_jsonl(const cos_v181_state_t *s,
                              char *buf, size_t cap);

int cos_v181_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V181_AUDIT_H */
