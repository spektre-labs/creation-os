/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef HTTP_CHAT_H
#define HTTP_CHAT_H

#include <stddef.h>

/** Minimal blocking HTTP server (POSIX). Returns 0 on clean shutdown request. */
int cos_http_chat_serve(unsigned short port,
                        int (*complete)(const char *prompt, char *out, size_t cap, void *ctx), void *ctx);

#endif /* HTTP_CHAT_H */
