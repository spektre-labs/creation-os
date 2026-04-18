/*
 * v167 σ-Governance-API — CLI entry point.
 *
 *   creation_os_v167_governance --self-test
 *   creation_os_v167_governance                     # state JSON (after
 *                                                   # a scripted scenario)
 *   creation_os_v167_governance --audit             # NDJSON audit log
 *
 * The default run executes a deterministic scripted scenario:
 *   - init spektre-labs with 3 policies + 4 nodes
 *   - evaluate 4 prompts (medical/creative/code/medical-denied-for-auditor)
 *   - upsert a tightened medical policy
 *   - push to fleet (bumps policy_version on all nodes)
 *   - mark cloud-b health=down, push again, assert it's skipped
 *   - rollback adapter on all nodes to adapter-v1.1.0
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "governance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scripted(cos_v167_state_t *s) {
    cos_v167_init(s, "spektre-labs");
    cos_v167_evaluate(s, "alice", COS_V167_ROLE_USER,     "lab-m3",   "medical",  "Is aspirin safe?", 0.40f);
    cos_v167_evaluate(s, "alice", COS_V167_ROLE_USER,     "lab-m3",   "creative", "write a poem",     0.80f);
    cos_v167_evaluate(s, "bob",   COS_V167_ROLE_DEVELOPER,"cloud-a",  "code",     "refactor",         0.20f);
    cos_v167_evaluate(s, "carol", COS_V167_ROLE_AUDITOR,  "cloud-b",  "creative", "poem again",       0.10f);

    cos_v167_policy_t tightened = {0};
    strncpy(tightened.domain, "medical", sizeof(tightened.domain) - 1);
    tightened.tau = 0.25f;
    tightened.has_abstain_message = true;
    strncpy(tightened.abstain_message, "Not medical advice.",
            sizeof(tightened.abstain_message) - 1);
    tightened.require_sandbox = true;
    cos_v167_policy_upsert(s, &tightened);
    cos_v167_fleet_push_policy(s);

    cos_v167_fleet_set_health(s, "cloud-b", COS_V167_HEALTH_DOWN);
    cos_v167_policy_t legal = {0};
    strncpy(legal.domain, "legal", sizeof(legal.domain) - 1);
    legal.tau = 0.20f;
    legal.has_abstain_message = true;
    strncpy(legal.abstain_message, "Consult a lawyer.",
            sizeof(legal.abstain_message) - 1);
    legal.require_sandbox = true;
    cos_v167_policy_upsert(s, &legal);
    cos_v167_fleet_push_policy(s);

    cos_v167_fleet_rollback(s, "adapter-v1.1.0");
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v167_self_test();
        if (rc == 0) puts("v167 self-test: ok");
        else         fprintf(stderr, "v167 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v167_state_t s;
    scripted(&s);

    if (argc >= 2 && strcmp(argv[1], "--audit") == 0) {
        char buf[16384];
        size_t n = cos_v167_audit_to_ndjson(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v167: ndjson overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        return 0;
    }

    char buf[8192];
    size_t n = cos_v167_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v167: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout);
    putchar('\n');
    return 0;
}
