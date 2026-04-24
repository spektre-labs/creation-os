/*
 * Constitutional runtime — Codex-derived rules checked on every output.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CONSTITUTION_H
#define COS_SIGMA_CONSTITUTION_H

#include <stdint.h>

struct cos_proof_receipt;

#ifdef __cplusplus
extern "C" {
#endif

struct cos_constitution_rule {
    char  id[16];
    char  text[512];
    char  tag[16]; /* e.g. TRUTH, ERROR — drives evaluator */
    float sigma_threshold;
    int   mandatory; /* 1 = halt if violated */
    int   violations;
    int   checks;
};

struct cos_constitution {
    struct cos_constitution_rule rules[32];
    int    n_rules;
    int    total_checks;
    int    total_violations;
    int    total_halts;
    uint8_t codex_hash[32];
    int    initialised;
};

/** Load Codex from path (NULL → data/codex/atlantean_codex_compact.txt).
 *  Parses # RULE|… lines; SHA-256 of file bytes stored in constitution.
 *  Returns 0 on success, negative on I/O error (still installs safe
 *  minimal rules when possible). */
int cos_constitution_init(const char *codex_path);

const struct cos_constitution *cos_constitution_get(void);

/** Evaluate all rules; sets *violations to total rule hits and
 *  *mandatory_violations to mandatory-only hits. violation_report is a
 *  short human summary (first failure). */
int cos_constitution_check(const char *output, float sigma,
                           const struct cos_proof_receipt *receipt,
                           int *violations, int *mandatory_violations,
                           char *violation_report, int max_len);

/** 0 = OK, 1 = advisory only, 2 = mandatory halt */
int cos_constitution_enforce(const struct cos_constitution *c,
                             const char *output, float sigma);

char *cos_constitution_report(const struct cos_constitution *c);
void cos_constitution_print_status(const struct cos_constitution *c);

/** Append one JSONL record per rule evaluation (audit trail). */
void cos_constitution_audit_log(const char *rule_id,
                                const uint8_t output_hash[32], float sigma,
                                int compliant, const char *action);

/** Read ~/.cos/constitution/audit.jsonl (last N lines). Returns malloc'd
 *  string or NULL. */
char *cos_constitution_audit_tail(int max_lines, int violations_only);

int cos_constitution_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CONSTITUTION_H */
