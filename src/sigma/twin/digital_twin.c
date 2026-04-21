/*
 * Digital twin — implementation (HORIZON-2).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "digital_twin.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static void trim_inplace(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static void twin_line(FILE *out, const char *msg) {
    if (out) fprintf(out, "[twin: %s]\n", msg);
}

static const char *path_basename(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void path_dirname_buf(const char *path, char *out, size_t cap) {
    if (!path || !out || cap < 4) {
        if (out && cap) snprintf(out, cap, ".");
        return;
    }
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", path);
    char *last = strrchr(tmp, '/');
    if (last == NULL) {
        snprintf(out, cap, ".");
        return;
    }
    if (last == tmp) {
        snprintf(out, cap, "/");
        return;
    }
    *last = '\0';
    snprintf(out, cap, "%s", tmp);
}

static int split_cmd(char *work, char **words, int maxw) {
    int nw = 0;
    char *p = work;
    while (*p && nw < maxw) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        words[nw++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p == '\0') break;
        *p++ = '\0';
    }
    return nw;
}

static int cp_pick_paths(char **words, int nw, const char **src, const char **dst) {
    const char *paths[8];
    int np = 0;
    for (int i = 1; i < nw && np < 8; i++) {
        if (words[i][0] == '-') continue;
        paths[np++] = words[i];
    }
    if (np != 2) return -1; /* exactly one source + one dest */
    *src = paths[0];
    *dst = paths[1];
    return 0;
}

static void resolve_dest_file(const char *src, const char *dst_raw,
                              char *out, size_t cap) {
    struct stat st;
    if (stat(dst_raw, &st) == 0 && S_ISDIR(st.st_mode))
        snprintf(out, cap, "%s/%s", dst_raw, path_basename(src));
    else
        snprintf(out, cap, "%s", dst_raw);
}

static void finalize_sigma(cos_digital_twin_report_t *rep) {
    if (rep->n_failed >= 2)
        rep->sigma_twin = 0.90f;
    else if (rep->n_failed == 1)
        rep->sigma_twin = 0.50f;
    else if (rep->n_warnings > 0)
        rep->sigma_twin = 0.35f;
    else
        rep->sigma_twin = 0.02f;
}

static int simulate_cp(const char *src, const char *dst_raw, FILE *out,
                       cos_digital_twin_report_t *rep) {
    struct stat st_src;
    char msg[512];
    char dest_path[512];
    char parent[512];

    resolve_dest_file(src, dst_raw, dest_path, sizeof dest_path);
    snprintf(rep->dest_resolved, sizeof rep->dest_resolved, "%s", dest_path);

    if (stat(src, &st_src) != 0) {
        rep->n_failed++;
        snprintf(msg, sizeof msg, "%s does NOT exist ✗", src);
        twin_line(out, msg);
        finalize_sigma(rep);
        return 0;
    }

    double mb = (double)st_src.st_size / (1024.0 * 1024.0);
    snprintf(msg, sizeof msg, "%s exists (%.2f MB) ✓", src, mb);
    twin_line(out, msg);

    path_dirname_buf(dest_path, parent, sizeof parent);
    if (parent[0] == '\0') snprintf(parent, sizeof parent, ".");

    if (access(parent, W_OK) != 0) {
        rep->n_failed++;
        snprintf(msg, sizeof msg, "%s not writable ✗", parent);
        twin_line(out, msg);
    } else {
        snprintf(msg, sizeof msg, "%s writable ✓", parent);
        twin_line(out, msg);
    }

    struct statvfs vfs;
    if (statvfs(parent, &vfs) != 0) {
        rep->n_warnings++;
        twin_line(out, "statvfs failed — disk space unknown (warning)");
    } else {
        uint64_t avail = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
        uint64_t need  = (uint64_t)st_src.st_size + 65536u;
        double gb_free = (double)avail / (1024.0 * 1024.0 * 1024.0);
        snprintf(msg, sizeof msg, "disk space %.2f GB free ✓", gb_free);
        twin_line(out, msg);
        if (avail < need) {
            rep->n_failed++;
            snprintf(msg, sizeof msg,
                     "insufficient free space (need ~%llu B) ✗",
                     (unsigned long long)need);
            twin_line(out, msg);
        }
    }

    int fd = open(src, O_RDONLY);
    if (fd >= 0) {
        if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
            int e = errno;
            if (e == EWOULDBLOCK || e == EAGAIN) {
                rep->n_warnings++;
                twin_line(out, "source file may be locked by another process (warning)");
            }
        }
        close(fd);
    }

    finalize_sigma(rep);
    rep->parsed_cp = 1;
    return 0;
}

int cos_digital_twin_simulate(const char *shell_cmd, FILE *out,
                              cos_digital_twin_report_t *rep) {
    if (!rep) return -1;
    memset(rep, 0, sizeof *rep);
    if (!shell_cmd) return -1;

    char buf[2048];
    snprintf(buf, sizeof buf, "%s", shell_cmd);
    trim_inplace(buf);
    if (buf[0] == '\0') return -1;

    twin_line(out, "checking preconditions...");

    char work[2048];
    snprintf(work, sizeof work, "%s", buf);
    char *words[64];
    int nw = split_cmd(work, words, 64);
    if (nw < 1) return -1;

    if (strcasecmp(words[0], "cp") != 0) {
        char u[256];
        snprintf(u, sizeof u,
                 "verb '%s' — no deep twin model (manual review) ✗", words[0]);
        twin_line(out, u);
        rep->sigma_twin = 0.45f;
        rep->parsed_cp  = 0;
        return 0;
    }

    const char *src, *dst;
    if (cp_pick_paths(words, nw, &src, &dst) != 0) {
        twin_line(out, "cp: need exactly SRC DST (after flags) ✗");
        rep->n_failed   = 1;
        rep->sigma_twin = 0.55f;
        return 0;
    }

    return simulate_cp(src, dst, out, rep);
}

int cos_digital_twin_self_test(void) {
    char src[128], dst[128], bad[128];
    snprintf(src, sizeof src, "/tmp/cos_twin_selftest_src_%d", (int)getpid());
    snprintf(dst, sizeof dst, "/tmp/cos_twin_selftest_dst_%d", (int)getpid());
    snprintf(bad, sizeof bad, "/tmp/cos_twin_SELFTEST_MISSING_%d", (int)getpid());

    FILE *f = fopen(src, "w");
    if (!f) return 1;
    fprintf(f, "payload-for-twin\n");
    fclose(f);

    cos_digital_twin_report_t r;
    char cmd[300];
    snprintf(cmd, sizeof cmd, "cp %s %s", src, dst);
    if (cos_digital_twin_simulate(cmd, NULL, &r) != 0) { unlink(src); return 2; }
    if (!r.parsed_cp || r.n_failed != 0) { unlink(src); return 3; }
    if (!(r.sigma_twin < 0.15f)) { unlink(src); return 4; }

    snprintf(cmd, sizeof cmd, "cp %s %s", bad, dst);
    memset(&r, 0, sizeof r);
    if (cos_digital_twin_simulate(cmd, NULL, &r) != 0) { unlink(src); return 5; }
    if (r.n_failed < 1 || r.sigma_twin < 0.45f) { unlink(src); return 6; }

    memset(&r, 0, sizeof r);
    if (cos_digital_twin_simulate("mv a b", NULL, &r) != 0) { unlink(src); return 7; }
    if (r.parsed_cp) { unlink(src); return 8; }
    if (r.sigma_twin < 0.40f) { unlink(src); return 9; }

    unlink(src);
    if (access(dst, F_OK) == 0) unlink(dst);
    return 0;
}
