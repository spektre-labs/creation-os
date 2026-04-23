/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Unified σ-native agent node (MCP + A2A surfaces).
 */
#ifndef COS_AGENT_NODE_H
#define COS_AGENT_NODE_H

#include <stdint.h>

#define COS_AGENT_NODE_NA (-999)

struct cos_agent_identity {
    uint8_t codex_hash[32];
    uint8_t license_hash[32];
    char    agent_name[64];
    float   sigma_mean_lifetime;
    int     total_interactions;
    int     quarantined_count;
};

/* Returns COS_AGENT_NODE_NA when argv does not select a node subcommand. */
int cos_agent_node_cli(int argc, char **argv);

int cos_agent_identity_fill(struct cos_agent_identity *out);

int cos_agent_node_self_test(void);

#endif /* COS_AGENT_NODE_H */
