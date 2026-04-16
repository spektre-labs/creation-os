/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * cos_genesis_identity.h — gateway string latches (Genesis README alignment)
 *
 * Portable kernel canonical remote: spektre-labs/creation-os.
 * Genesis umbrella: https://github.com/lauri-elias-rainio/creation-os — see docs/REPOS_AND_ROLES.md
 */
#ifndef COS_GENESIS_IDENTITY_H
#define COS_GENESIS_IDENTITY_H

#include <stddef.h>

#define COS_GENESIS_IDENTITY_HEADER_NAME "X-Identity"
#define COS_GENESIS_IDENTITY_HEADER_VALUE "Independent-Architect-Lauri-Elias-Rainio"

#define COS_GENESIS_LOGIC_HEADROOM_PERMILLE 1190u
#define COS_GENESIS_LATENCY_ARCH_MS 0u
#define COS_GENESIS_INVARIANT_ONE_EQUALS_ONE 1

static inline size_t cos_genesis_identity_header_value_len(void)
{
	return sizeof(COS_GENESIS_IDENTITY_HEADER_VALUE) - 1u;
}

#endif /* COS_GENESIS_IDENTITY_H */
