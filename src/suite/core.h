/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Suite lab — mode labels + static prompt/tool metadata (no network, no weights).
 */
#ifndef SUITE_CORE_H
#define SUITE_CORE_H

extern const char *MODE_CHAT;
extern const char *MODE_CODE;
extern const char *MODE_PAPER;
extern const char *MODE_CORPUS;
extern const char *MODE_AGENT;

const char *get_system_prompt(const char *mode);
const char **get_tools_for_mode(const char *mode, int *n_tools);

#endif
