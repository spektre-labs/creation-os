/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "mcp_sigma.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void cos_mcp_http_serve(unsigned short port)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return;
    }
    int one = 1;
    (void)setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) != 0) {
        perror("bind");
        close(srv);
        return;
    }
    if (listen(srv, 4) != 0) {
        perror("listen");
        close(srv);
        return;
    }

    fprintf(stderr, "creation_os_mcp: HTTP JSON-RPC on http://127.0.0.1:%u (POST body = one JSON-RPC line; no SSE yet)\n",
        (unsigned)port);

    for (;;) {
        int c = accept(srv, NULL, NULL);
        if (c < 0) {
            perror("accept");
            continue;
        }
        char req[65536];
        ssize_t nr = recv(c, req, sizeof req - 1u, 0);
        if (nr <= 0) {
            close(c);
            continue;
        }
        req[nr] = '\0';
        const char *body = strstr(req, "\r\n\r\n");
        if (!body) {
            const char *bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            (void)send(c, bad, (int)strlen(bad), 0);
            close(c);
            continue;
        }
        body += 4;

        char out[512 * 1024];
        size_t n = cos_mcp_handle_request(body, out, sizeof out);
        if (!n) {
            const char *bad = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
            (void)send(c, bad, (int)strlen(bad), 0);
            close(c);
            continue;
        }

        char hdr[256];
        int hl = snprintf(hdr, sizeof hdr,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            strlen(out));
        if (hl > 0 && hl < (int)sizeof hdr)
            (void)send(c, hdr, (size_t)hl, 0);
        (void)send(c, out, strlen(out), 0);
        close(c);
    }
}
