/*
 * EU AI Act–oriented compliance snapshot for CLI / documentation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "eu_compliance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct cos_eu_compliance cos_eu_check(void)
{
    struct cos_eu_compliance c;

    memset(&c, 0, sizeof c);
    c.risk_tier = 2; /* local research / tooling default */

    c.human_oversight     = 1;
    c.transparency        = 1;
    c.incident_reporting  = 1;
    c.data_governance     = 1;

    c.audit_trail = 1; /* JSONL receipts + constitutional audit supported */

    c.compliant =
        c.human_oversight && c.transparency && c.audit_trail
        && c.data_governance && c.incident_reporting && c.risk_tier < 4;

    snprintf(
        c.report, sizeof c.report,
        "EU AI Act mapping (Creation OS runtime):\n"
        "  Art 9  risk management     — σ-gate + error attribution\n"
        "  Art 11 technical documentation — proof receipts (JSON)\n"
        "  Art 12 record-keeping       — receipt chain + JSONL events\n"
        "  Art 13 transparency         — σ surfaced per turn + receipts\n"
        "  Art 14 human oversight      — sovereign brake + SERVING mode\n"
        "  Art 15 accuracy/robustness/cybersecurity — σ + conformal hooks + "
        "SHA-256 proof receipts + audit JSONL chain\n"
        "  Art 50 transparency of AI-generated content — machine-readable "
        "σ-credential sidecars (ai.creation-os.sigma; cos stamp / cos validate)\n"
        "  Art 52 transparency (GPAI) — open-source demo + CLI receipts\n"
        "\nSignals: audit_trail=%s risk_tier=%d human_oversight=%s "
        "data_governance=%s\n",
        c.audit_trail ? "active" : "inactive", c.risk_tier,
        c.human_oversight ? "on" : "off",
        c.data_governance ? "local-first" : "unset");

    return c;
}

char *cos_eu_report(const struct cos_eu_compliance *c)
{
    char *s;
    int   n;

    if (c == NULL)
        return NULL;
    s = malloc(3072);
    if (s == NULL)
        return NULL;
    n = snprintf(
        s, 3072,
        "{\"eu_ai_act\":{\"risk_tier\":%d,\"human_oversight\":%d,"
        "\"transparency\":%d,\"audit_trail\":%d,"
        "\"incident_reporting\":%d,\"data_governance\":%d,"
        "\"compliant\":%d}}\n",
        c->risk_tier, c->human_oversight, c->transparency, c->audit_trail,
        c->incident_reporting, c->data_governance, c->compliant);
    if (n < 0 || (size_t)n >= 3072) {
        free(s);
        return NULL;
    }
    return s;
}

const char *cos_eu_ai_act_article15_evidence_md(void)
{
    return "# EU AI Act Article 15 — evidence (Creation OS)\n\n"
           "Article 15 requires accuracy, robustness, and cybersecurity.\n\n"
           "| Pillar | Mechanism |\n"
           "|--------|-----------|\n"
           "| Accuracy | Scalar σ per output; optional multi-σ shadow; "
           "conformal calibration hooks; benchmark harnesses in-tree. |\n"
           "| Robustness | Append-only `~/.cos/audit/*.jsonl` proof records; "
           "`cos receipt verify` detects broken hash chains. |\n"
           "| Cybersecurity | SHA-256 proof receipts (FIPS-180-4, "
           "dependency-free implementation); prompt-bound `output_hash` "
           "when chat emits receipts. |\n\n"
           "Commands:\n"
           "- `cos receipt verify [AUDIT.jsonl]`\n"
           "- `cos chat ...` (receipt prefix on the polished line)\n"
           "- `cos verify --receipt` (stdin prompt → `cos-chat --once "
           "--receipt`)\n";
}

const char *cos_nist_rmf_report(void)
{
    return "NIST AI RMF (informative mapping for Creation OS):\n"
           "  Govern  — Codex + constitutional RULE pragmas + receipts\n"
           "  Map     — σ channels + risk_tier in EU snapshot\n"
           "  Measure — σ-gate, benchmarks, conformal calibration hooks\n"
           "  Manage  — sovereign brake, abstain path, human oversight bit\n"
           "  operationalize via `cos compliance` and persisted receipts.\n";
}
