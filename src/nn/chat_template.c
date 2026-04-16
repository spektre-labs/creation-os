/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "chat_template.h"

#include <stdio.h>
#include <string.h>

int cos_chat_apply_llama3(const char *system, const char *user, char *out, size_t cap)
{
    if (!user || !out || cap < 16u)
        return -1;
    const char *sys = system && system[0] ? system : "You are a helpful assistant.";
    int n = snprintf(out, cap,
                     "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n"
                     "%s\n"
                     "<|start_header_id|>user<|end_header_id|>\n"
                     "%s\n"
                     "<|start_header_id|>assistant<|end_header_id|>\n",
                     sys, user);
    if (n < 0 || (size_t)n >= cap)
        return -1;
    return n;
}
