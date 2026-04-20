/*
 * cos-mesh-node — minimal TCP σ-Protocol node for 2-node integration.
 *
 * FIX-6: localhost != real network; we prove real-network semantics
 * by spawning two processes on two different ports that exchange
 * actual σ-Protocol frames over actual TCP sockets.  The protocol
 * codec and the Ed25519-capable signer contract are shared with the
 * single-binary D-series demos.
 *
 * Usage:
 *
 *   cos-mesh-node --port <P> [--peer HOST:P] [--send QUERY|HEARTBEAT|UNLEARN]
 *                 [--payload "text"] [--duration-ms N]
 *
 * Behaviour:
 *
 *   1. Binds a TCP listener on 127.0.0.1:<port>.
 *   2. If --peer is given, connects to the peer, sends a single
 *      --send frame (default HEARTBEAT), reads the peer's response
 *      (if any), closes the connection.
 *   3. Services incoming connections: reads one frame, verifies
 *      the signature with the shared test key, emits a JSON event
 *      for every received frame, and if the frame is a QUERY it
 *      writes a RESPONSE frame back.
 *   4. Exits cleanly after --duration-ms (default 1500).
 *
 * This is intentionally minimal — no threads, no async, no retries.
 * One listener poll loop, one optional outbound send.  Good enough
 * to prove the protocol survives real sockets, and fast enough to
 * run in CI in ~2 seconds.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Fixed shared secret for the smoke test.  Production nodes use
 * Ed25519 via cos_sigma_proto_ed25519_{sign,verify}. */
static const uint8_t MESH_KEY[] = "creation-os-mesh-test-key";

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

/* Parse "host:port" into sockaddr_in.  Only IPv4 for the smoke test. */
static int parse_endpoint(const char *s, struct sockaddr_in *out) {
    char host[128];
    const char *colon = strrchr(s, ':');
    if (!colon) return -1;
    size_t hl = (size_t)(colon - s);
    if (hl >= sizeof host) return -2;
    memcpy(host, s, hl); host[hl] = 0;
    int port = atoi(colon + 1);
    if (port <= 0 || port > 65535) return -3;
    memset(out, 0, sizeof *out);
    out->sin_family = AF_INET;
    out->sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &out->sin_addr) != 1) return -4;
    return 0;
}

static int send_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = buf;
    while (n) {
        ssize_t w = send(fd, p, n, 0);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += (size_t)w; n -= (size_t)w;
    }
    return 0;
}

/* Read up to N bytes with a total timeout.  Returns bytes read or -1. */
static ssize_t recv_with_timeout(int fd, void *buf, size_t n, int timeout_ms) {
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    size_t got = 0;
    while (got < n) {
        int r = poll(&pfd, 1, timeout_ms);
        if (r <= 0) break;
        ssize_t k = recv(fd, (uint8_t*)buf + got, n - got, 0);
        if (k <= 0) break;
        got += (size_t)k;
    }
    return (ssize_t)got;
}

static void emit_event(const char *role, int port, const char *event,
                       const char *detail) {
    printf("{\"role\":\"%s\",\"port\":%d,\"event\":\"%s\",\"detail\":\"%s\","
           "\"ts_ns\":%llu}\n",
           role, port, event, detail,
           (unsigned long long)now_ns());
    fflush(stdout);
}

static int send_frame_to_peer(const char *peer, cos_msg_type_t type,
                              const char *sender_id,
                              const char *payload,
                              const char *role, int our_port) {
    struct sockaddr_in sa;
    if (parse_endpoint(peer, &sa) != 0) return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    if (connect(s, (struct sockaddr*)&sa, sizeof sa) != 0) {
        emit_event(role, our_port, "connect_failed", strerror(errno));
        close(s); return -1;
    }
    cos_msg_t tx = { 0 };
    tx.type         = type;
    strncpy(tx.sender_id, sender_id, COS_MSG_ID_CAP - 1);
    tx.sender_sigma = 0.12f;
    tx.timestamp_ns = now_ns();
    tx.payload      = payload ? (const uint8_t*)payload : (const uint8_t*)"";
    tx.payload_len  = payload ? (uint32_t)strlen(payload) : 0;

    uint8_t buf[4096];
    size_t w = 0;
    if (cos_sigma_proto_encode(&tx, MESH_KEY, sizeof MESH_KEY - 1,
                               buf, sizeof buf, &w) != 0) {
        close(s); return -1;
    }
    if (send_all(s, buf, w) != 0) { close(s); return -1; }
    emit_event(role, our_port, "send_ok",
               cos_sigma_proto_type_name(type));

    /* Await response for QUERY only. */
    if (type == COS_MSG_QUERY) {
        uint8_t rbuf[4096];
        ssize_t got = recv_with_timeout(s, rbuf, sizeof rbuf, 500);
        if (got > 0) {
            cos_msg_t rx;
            if (cos_sigma_proto_decode(rbuf, (size_t)got,
                                       MESH_KEY, sizeof MESH_KEY - 1,
                                       &rx) == 0) {
                emit_event(role, our_port, "recv_response",
                           cos_sigma_proto_type_name(rx.type));
            } else {
                emit_event(role, our_port, "recv_decode_failed", "");
            }
        }
    }
    close(s);
    return 0;
}

int main(int argc, char **argv) {
    int port = 0;
    const char *peer = NULL;
    const char *send_kind = "HEARTBEAT";
    const char *payload = "ping";
    const char *role = "node";
    int duration_ms = 1500;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--peer") == 0 && i + 1 < argc) peer = argv[++i];
        else if (strcmp(argv[i], "--send") == 0 && i + 1 < argc) send_kind = argv[++i];
        else if (strcmp(argv[i], "--payload") == 0 && i + 1 < argc) payload = argv[++i];
        else if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--duration-ms") == 0 && i + 1 < argc) duration_ms = atoi(argv[++i]);
        else {
            fprintf(stderr, "cos-mesh-node: unknown arg '%s'\n", argv[i]);
            return 2;
        }
    }
    if (port <= 0) {
        fprintf(stderr, "cos-mesh-node: --port required\n");
        return 2;
    }

    /* Listener. */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }
    int one = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in la = { 0 };
    la.sin_family = AF_INET;
    la.sin_port   = htons((uint16_t)port);
    la.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listen_fd, (struct sockaddr*)&la, sizeof la) != 0) {
        perror("bind"); close(listen_fd); return 1;
    }
    if (listen(listen_fd, 8) != 0) {
        perror("listen"); close(listen_fd); return 1;
    }
    set_nonblock(listen_fd);
    emit_event(role, port, "listen_ok", "");

    /* Outbound ping, if any. */
    if (peer) {
        cos_msg_type_t kind = COS_MSG_HEARTBEAT;
        if      (strcmp(send_kind, "QUERY") == 0)     kind = COS_MSG_QUERY;
        else if (strcmp(send_kind, "HEARTBEAT") == 0) kind = COS_MSG_HEARTBEAT;
        else if (strcmp(send_kind, "UNLEARN") == 0)   kind = COS_MSG_UNLEARN;
        else if (strcmp(send_kind, "CAPABILITY") == 0) kind = COS_MSG_CAPABILITY;
        send_frame_to_peer(peer, kind, role, payload, role, port);
    }

    /* Accept / serve loop until duration expires. */
    uint64_t start = now_ns();
    uint64_t deadline_ns = start + (uint64_t)duration_ms * 1000000ULL;
    int n_recv = 0;
    while (now_ns() < deadline_ns) {
        struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };
        int r = poll(&pfd, 1, 100);
        if (r <= 0) continue;
        int conn = accept(listen_fd, NULL, NULL);
        if (conn < 0) continue;
        uint8_t buf[4096];
        ssize_t got = recv_with_timeout(conn, buf, sizeof buf, 300);
        if (got <= 0) { close(conn); continue; }
        cos_msg_t rx;
        int rc = cos_sigma_proto_decode(buf, (size_t)got,
                                        MESH_KEY, sizeof MESH_KEY - 1, &rx);
        if (rc == 0) {
            ++n_recv;
            emit_event(role, port, "recv_ok",
                       cos_sigma_proto_type_name(rx.type));
            if (rx.type == COS_MSG_QUERY) {
                cos_msg_t tx = { 0 };
                tx.type         = COS_MSG_RESPONSE;
                strncpy(tx.sender_id, role, COS_MSG_ID_CAP - 1);
                tx.sender_sigma = 0.10f;
                tx.timestamp_ns = now_ns();
                tx.payload      = (const uint8_t*)"pong";
                tx.payload_len  = 4;
                uint8_t reply[512]; size_t w = 0;
                if (cos_sigma_proto_encode(&tx, MESH_KEY, sizeof MESH_KEY - 1,
                                           reply, sizeof reply, &w) == 0) {
                    send_all(conn, reply, w);
                    emit_event(role, port, "send_response", "RESPONSE");
                }
            }
        } else {
            emit_event(role, port, "recv_decode_failed", strerror(errno));
        }
        close(conn);
    }

    close(listen_fd);
    printf("{\"role\":\"%s\",\"port\":%d,\"event\":\"exit\","
           "\"received\":%d,\"duration_ms\":%d}\n",
           role, port, n_recv, duration_ms);
    fflush(stdout);
    return 0;
}
