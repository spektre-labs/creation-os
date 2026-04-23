/* Self-test driver for src/sigma/spike_engine.c (6+ cases). */
#include "spike_engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define T(name, cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", name); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    if (cos_spike_engine_self_test() != 0)
        return 1;

    {
        struct cos_spike_engine e;
        float v0[8], v1[8];
        int   f[8], n, i;
        T("init0", cos_spike_engine_init(&e) == 0);
        for (i = 0; i < 8; ++i) {
            v0[i] = 0.4f;
            v1[i] = 0.9f;
        }
        T("chk0", cos_spike_check(&e, v0, f, &n) == 0 && n == 0);
        T("chk1", cos_spike_check(&e, v1, f, &n) == 0 && n > 0);

        cos_spike_engine_stats(&e, NULL, NULL, NULL);
        T("stats_ok", e.total_fires >= 1);
    }

    {
        float es;
        int tf, ts;
        struct cos_spike_engine e;
        cos_spike_engine_init(&e);
        float fv[8];
        cos_spike_fill_channels_from_speculative(0.05f, 0.8f, fv);
        cos_spike_engine_stats(&e, &es, &tf, &ts);
        T("ratio", es >= 0.f && es <= 1.f);
    }

    {
        struct cos_spike_engine e;
        cos_spike_engine_init(&e);
        char tmpl[] = "/tmp/spk_ck_XXXXXX";
        int fd = mkstemp(tmpl);
        int rc;
        float v[8];
        int f[8], n;
        int j;
        T("mk", fd >= 0);
        close(fd);
        for (j = 0; j < 8; ++j)
            v[j] = 0.33f;
        cos_spike_check(&e, v, f, &n);
        rc = cos_spike_engine_snapshot_write(&e, tmpl);
        T("snap_w", rc == 0);
        memset(&e, 0, sizeof(e));
        cos_spike_engine_init(&e);
        rc = cos_spike_engine_snapshot_read(&e, tmpl);
        remove(tmpl);
        T("snap_r", rc == 0);
    }

    return 0;
}
