/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v33: validator-first tool JSON (subset of JSON Schema keywords used in-repo).
 */
#ifndef CREATION_OS_V33_SCHEMA_VALIDATOR_H
#define CREATION_OS_V33_SCHEMA_VALIDATOR_H

/** Returns 1 if valid, 0 if invalid. */
int cos_schema_validate_tool_json(const char *tool_json, const char *schema_path);

#endif /* CREATION_OS_V33_SCHEMA_VALIDATOR_H */
