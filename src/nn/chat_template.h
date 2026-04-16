/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef COS_CHAT_TEMPLATE_H
#define COS_CHAT_TEMPLATE_H

#include <stddef.h>

/** Minimal Llama-3-ish chat framing (plain text, not HF token template). */
int cos_chat_apply_llama3(const char *system, const char *user, char *out, size_t cap);

#endif /* COS_CHAT_TEMPLATE_H */
