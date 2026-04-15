#include "genesis_socket.h"

#include "../bare_metal/bbhash.h"
#include "../bare_metal/gda_hash.h"
#include "../kernel/living_weights.h"
#include "../kernel/shadow_ledger.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

enum {
    TIER_BBHASH    = 0,
    TIER_SOFTWARE  = 1,
    TIER_SHALLOW   = 2,
    TIER_MID       = 3,
    TIER_FULL      = 4,
    TIER_REJECT    = 5,
};

static uint32_t sigma_popcount_query(const uint8_t *q, size_t len) {
    uint32_t c = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = q[i];
        for (int j = 0; j < 8; j++)
            c += (uint32_t)((b >> j) & 1u);
    }
    return c;
}

static uint8_t tier_from_sigma(uint32_t sig) {
    uint32_t m = (sig == 0u) ? 0u : 1u;
    m |= (sig <= 8u) ? 0u : 2u;
    if (sig > 64u)
        return TIER_REJECT;
    if (sig > 32u)
        return TIER_FULL;
    if (sig > 16u)
        return TIER_MID;
    if (sig > 8u)
        return TIER_SHALLOW;
    return (uint8_t)(TIER_SOFTWARE * m + TIER_BBHASH * (1u - m));
}

static int read_exact(int fd, void *dst, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    size_t g = 0;
    while (g < n) {
        ssize_t r = read(fd, p + g, n - g);
        if (r <= 0)
            return -1;
        g += (size_t)r;
    }
    return 0;
}

static int write_exact(int fd, const void *src, size_t n) {
    const uint8_t *p = (const uint8_t *)src;
    size_t g = 0;
    while (g < n) {
        ssize_t w = write(fd, p + g, n - g);
        if (w <= 0)
            return -1;
        g += (size_t)w;
    }
    return 0;
}

static void handle_one(int cfd, bbhash_mmap_t *bb) {
    uint8_t lbuf[4];
    if (read_exact(cfd, lbuf, 4) != 0)
        return;
    uint32_t len = ((uint32_t)lbuf[0] << 24) | ((uint32_t)lbuf[1] << 16) | ((uint32_t)lbuf[2] << 8) |
                   (uint32_t)lbuf[3];
    if (len == 0 || len > 1024 * 1024)
        return;
    uint8_t *q = (uint8_t *)malloc(len);
    if (!q)
        return;
    if (read_exact(cfd, q, len) != 0) {
        free(q);
        return;
    }

    uint64_t key = 0;
    (void)gda_hash_query(q, len, &key);

    uint8_t tier = TIER_SOFTWARE;
    uint32_t sig = sigma_popcount_query(q, len);
    char ans[512];
    size_t ans_len = 0;

    uint32_t bb_val = 0;
    if (bb && bbhash_lookup_u64(bb, key, &bb_val)) {
        tier = TIER_BBHASH;
        sig = 0u;
        ans_len = (size_t)snprintf(ans, sizeof ans, "1=1 BBHash val=%u key=%016llx", bb_val,
                                   (unsigned long long)key);
    } else {
        tier = tier_from_sigma(sig);
        if (len >= 4u) {
            uint32_t tid = (uint32_t)q[0] | ((uint32_t)q[1] << 8) | ((uint32_t)q[2] << 16) |
                           ((uint32_t)q[3] << 24);
            lw_soc_touch(tid, sig < 16u);
            uint32_t sh = shadow_ledger_get(tid);
            (void)sh;
        }
        ans_len = (size_t)snprintf(ans, sizeof ans, "1=1 tier=%u sigma=%u key=%016llx", (unsigned)tier,
                                   (unsigned)sig, (unsigned long long)key);
    }

    if (ans_len >= sizeof ans)
        ans_len = sizeof ans - 1;

    uint8_t hdr[5];
    hdr[0] = tier;
    hdr[1] = (uint8_t)((sig >> 24) & 0xffu);
    hdr[2] = (uint8_t)((sig >> 16) & 0xffu);
    hdr[3] = (uint8_t)((sig >> 8) & 0xffu);
    hdr[4] = (uint8_t)(sig & 0xffu);
    (void)write_exact(cfd, hdr, sizeof hdr);
    (void)write_exact(cfd, ans, ans_len);

    free(q);
}

static const char *resolve_path(const char *override) {
    const char *e = getenv("CREATION_OS_GENESIS_SOCK");
    if (override && *override)
        return override;
    if (e && *e)
        return e;
    return "/var/run/creation.sock";
}

static int bind_unix_listen(const char *path) {
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0)
        return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    unlink(addr.sun_path);
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv);
        return -1;
    }
    if (listen(srv, 128) < 0) {
        close(srv);
        return -1;
    }
    return srv;
}

int genesis_socket_serve_forever(const char *path_override, const char *bbhash_mmap_path) {
    const char *want = resolve_path(path_override);
    char bound_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    bound_path[0] = 0;

    bbhash_mmap_t bb;
    memset(&bb, 0, sizeof(bb));
    if (bbhash_mmap_path && *bbhash_mmap_path)
        (void)bbhash_open_mmap(&bb, bbhash_mmap_path);

    shadow_ledger_init();
    lw_soc_init(32000);

    int srv = bind_unix_listen(want);
    if (srv < 0 && strcmp(want, "/tmp/creation.sock") != 0) {
        fprintf(stderr, "genesis: bind %s failed (%s), trying /tmp/creation.sock\n", want,
                strerror(errno));
        srv = bind_unix_listen("/tmp/creation.sock");
        if (srv >= 0)
            strncpy(bound_path, "/tmp/creation.sock", sizeof(bound_path) - 1);
    } else if (srv >= 0) {
        strncpy(bound_path, want, sizeof(bound_path) - 1);
    }

    if (srv < 0) {
        bbhash_close_mmap(&bb);
        return -1;
    }

    fprintf(stderr, "genesis: listening %s\n", bound_path[0] ? bound_path : want);

    for (;;) {
        int c = accept(srv, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        handle_one(c, &bb);
        close(c);
    }

    close(srv);
    bbhash_close_mmap(&bb);
    return 0;
}
