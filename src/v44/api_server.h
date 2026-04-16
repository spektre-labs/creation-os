/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v44 lab: minimal OpenAI-shaped HTTP handler for the σ-proxy (loopback POSIX).
 */
#ifndef CREATION_OS_V44_API_SERVER_H
#define CREATION_OS_V44_API_SERVER_H

/** Handle one accepted client socket (HTTP/1.1, Connection: close). */
int v44_api_handle_one(int client_fd);

#endif /* CREATION_OS_V44_API_SERVER_H */
