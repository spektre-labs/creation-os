/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *
 *  src/sigma/pipeline/health_live.c
 *  --------------------------------------------------------------------
 *  DEV-7: enrich cos_health_report_t with live runtime metrics.
 *
 *  Sources:
 *    1. ~/.cos/engram.db  (DEV-3 SQLite cache)
 *       → engrams (row count)
 *       → sigma_mean_last_100 (mean over newest 100 rows)
 *
 *    2. ~/.cos/distill_pairs.jsonl  (DEV-5 escalation log)
 *       → distill_pairs_today + cost_today_eur
 *         (rows whose "ts" field is ≥ midnight today local-time)
 *
 *    3. HTTP GET http://$COS_BITNET_SERVER_HOST:$COS_BITNET_SERVER_PORT/health
 *       → llama_server_up (1 if 200 + body contains "ok")
 *       → llama_server_host / port / model_path
 *
 *  Any single source can fail silently; we just leave that facet at
 *  whatever value init_defaults() seeded.  The caller is expected to
 *  call this after init_defaults() and before grade() + emit().
 *
 *  No libcurl dependency: /health is HTTP/1.1 + keep-it-simple, so
 *  we speak it via a raw POSIX socket (same as bitnet_server's
 *  bns_http_get helper, but standalone so cos-health stays light).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "health.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

/* --- path helpers ------------------------------------------------- */

static const char *resolve_home(void) {
    const char *h = getenv("HOME");
    if (h != NULL && h[0] != '\0') return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw != NULL && pw->pw_dir != NULL) ? pw->pw_dir : ".";
}

static void resolve_engram_db(char *out, size_t cap) {
    const char *ov = getenv("COS_ENGRAM_DB");
    if (ov != NULL && ov[0] != '\0') { snprintf(out, cap, "%s", ov); return; }
    snprintf(out, cap, "%s/.cos/engram.db", resolve_home());
}

static void resolve_distill_log(char *out, size_t cap) {
    const char *ov = getenv("CREATION_OS_DISTILL_LOG");
    if (ov != NULL && ov[0] != '\0') { snprintf(out, cap, "%s", ov); return; }
    snprintf(out, cap, "%s/.cos/distill_pairs.jsonl", resolve_home());
}

/* --- engram.db reader -------------------------------------------- */

static int load_engram_stats(cos_health_report_t *r) {
    char path[1024];
    resolve_engram_db(path, sizeof path);
    struct stat st;
    if (stat(path, &st) != 0) return -1;  /* no DB yet; leave defaults */

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -2;
    }

    /* Row count. */
    sqlite3_stmt *st1 = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM engram", -1, &st1, NULL) == SQLITE_OK) {
        if (sqlite3_step(st1) == SQLITE_ROW) {
            r->engrams = sqlite3_column_int(st1, 0);
        }
        sqlite3_finalize(st1);
    }

    /* σ mean over newest 100 entries. */
    sqlite3_stmt *st2 = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT AVG(sigma) FROM (SELECT sigma FROM engram "
            " ORDER BY ts DESC LIMIT 100)", -1, &st2, NULL) == SQLITE_OK) {
        if (sqlite3_step(st2) == SQLITE_ROW) {
            double v = sqlite3_column_double(st2, 0);
            if (v >= 0.0 && v <= 1.0) {
                r->sigma_mean_last_100 = (float)v;
            }
        }
        sqlite3_finalize(st2);
    }

    sqlite3_close(db);
    return 0;
}

/* --- distill log reader ------------------------------------------ */

/* Parse a line of the form {"ts":N,...,"cost_eur":N,...}.  Writes
 * ts → *out_ts and cost → *out_cost.  Returns 0 on success.  A
 * deliberately minimal scanner; escalation.c owns the canonical
 * writer so the shape is trusted. */
static int parse_distill_line(const char *s,
                              long long *out_ts, double *out_cost) {
    const char *ts = strstr(s, "\"ts\":");
    if (ts == NULL) return -1;
    *out_ts = strtoll(ts + 5, NULL, 10);

    const char *ce = strstr(s, "\"cost_eur\":");
    if (ce == NULL) { *out_cost = 0.0; return 0; }  /* ts-only row */
    *out_cost = strtod(ce + 11, NULL);
    return 0;
}

static int load_distill_today(cos_health_report_t *r) {
    char path[1024];
    resolve_distill_log(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (f == NULL) return -1;

    /* Midnight today, local time. */
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
    time_t midnight = mktime(&lt);

    char line[65536];
    int    count_today = 0;
    double sum_eur     = 0.0;
    while (fgets(line, sizeof line, f) != NULL) {
        long long ts = 0;
        double    c  = 0.0;
        if (parse_distill_line(line, &ts, &c) != 0) continue;
        if (ts >= (long long)midnight) {
            count_today++;
            sum_eur += c;
        }
    }
    fclose(f);
    r->distill_pairs_today = count_today;
    r->cost_today_eur      = (float)sum_eur;
    return 0;
}

/* --- raw HTTP /health probe -------------------------------------- */

static int tcp_connect_timeout(const char *host, int port, int timeout_s) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        close(fd); return -1;
    }
    struct timeval tv = { .tv_sec = timeout_s, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0) {
        close(fd); return -1;
    }
    return fd;
}

static int probe_llama_server(cos_health_report_t *r) {
    const char *host = getenv("COS_BITNET_SERVER_HOST");
    const char *port_s = getenv("COS_BITNET_SERVER_PORT");
    if (host == NULL || host[0] == '\0') host = "127.0.0.1";
    int port = (port_s != NULL && port_s[0] != '\0') ? atoi(port_s) : 8088;
    if (port <= 0) port = 8088;

    snprintf(r->llama_server_host, sizeof r->llama_server_host, "%s", host);
    r->llama_server_port = port;

    const char *model = getenv("COS_BITNET_SERVER_MODEL");
    if (model == NULL || model[0] == '\0')
        model = "./models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf";
    snprintf(r->model_path, sizeof r->model_path, "%s", model);

    int fd = tcp_connect_timeout(host, port, 1);
    if (fd < 0) { r->llama_server_up = 0; return -1; }

    char req[512];
    int rn = snprintf(req, sizeof req,
                      "GET /health HTTP/1.1\r\n"
                      "Host: %s:%d\r\n"
                      "Connection: close\r\n\r\n",
                      host, port);
    if (rn <= 0 || send(fd, req, (size_t)rn, 0) != rn) {
        close(fd); r->llama_server_up = 0; return -1;
    }

    char buf[2048] = {0};
    size_t total = 0;
    for (;;) {
        ssize_t n = recv(fd, buf + total, sizeof buf - 1 - total, 0);
        if (n <= 0) break;
        total += (size_t)n;
        if (total + 1 >= sizeof buf) break;
    }
    close(fd);
    buf[total] = '\0';

    int ok = (strncmp(buf, "HTTP/1.1 200", 12) == 0
              && strstr(buf, "\"ok\"") != NULL);
    r->llama_server_up = ok ? 1 : 0;
    return ok ? 0 : -1;
}

/* --- public entry ------------------------------------------------ */

int cos_sigma_health_load_live(cos_health_report_t *r) {
    if (r == NULL) return -1;
    /* Best-effort; all sub-loaders tolerate missing sources. */
    load_engram_stats(r);
    load_distill_today(r);
    probe_llama_server(r);
    return 0;
}
