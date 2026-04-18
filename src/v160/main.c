/*
 * v160 σ-Telemetry — CLI entry point.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "telemetry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fputs(
      "usage: creation_os_v160_telemetry --self-test\n"
      "       creation_os_v160_telemetry --trace   [--seed S] [--task T]\n"
      "       creation_os_v160_telemetry --metrics [--seed S]\n"
      "       creation_os_v160_telemetry --logs    [--seed S] [--task T]\n",
      stderr);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v160_self_test();
        printf("{\"kernel\":\"v160\",\"self_test\":\"%s\",\"rc\":%d}\n",
               rc == 0 ? "pass" : "fail", rc);
        return rc == 0 ? 0 : 1;
    }

    uint64_t    seed = 160;
    const char *task = "chat";
    enum { MODE_NONE, MODE_TRACE, MODE_METRICS, MODE_LOGS } mode = MODE_NONE;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--trace"))    mode = MODE_TRACE;
        else if (!strcmp(argv[i], "--metrics")) mode = MODE_METRICS;
        else if (!strcmp(argv[i], "--logs"))    mode = MODE_LOGS;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--task") && i + 1 < argc) {
            task = argv[++i];
        } else {
            return usage();
        }
    }
    if (mode == MODE_NONE) return usage();

    cos_v160_trace_t t;
    cos_v160_trace_init(&t, seed, task);

    if (mode == MODE_TRACE) {
        char buf[16384];
        cos_v160_trace_to_otlp_json(&t, buf, sizeof(buf));
        printf("%s\n", buf);
    } else if (mode == MODE_METRICS) {
        cos_v160_registry_t r;
        cos_v160_registry_init(&r);
        cos_v160_registry_put(&r, COS_V160_METRIC_GAUGE,
            "cos_sigma_product", "task", task, t.sigma_product);
        for (int i = 0; i < t.n_spans; ++i) {
            cos_v160_registry_put(&r, COS_V160_METRIC_GAUGE,
                "cos_sigma_channel", "channel", t.spans[i].name,
                t.spans[i].sigma);
        }
        cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
            "cos_abstain_total", "", "", 0.0);
        cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
            "cos_heal_total",    "", "", 0.0);
        cos_v160_registry_put(&r, COS_V160_METRIC_COUNTER,
            "cos_rsi_cycle_total", "", "", 0.0);
        cos_v160_registry_put(&r, COS_V160_METRIC_GAUGE,
            "cos_skill_count", "", "", 42.0);
        char prom[8192];
        cos_v160_registry_to_prometheus(&r, prom, sizeof(prom));
        fputs(prom, stdout);
    } else {
        cos_v160_log_ring_t lr;
        cos_v160_log_ring_init(&lr);
        cos_v160_log_ring_push(&lr, COS_V160_LOG_INFO, "v106",
            t.trace_id, t.sigma_product, "request begin",
            t.spans[0].start_unix_nano);
        for (int i = 0; i < t.n_spans; ++i) {
            char msg[COS_V160_MAX_MSG];
            snprintf(msg, sizeof(msg), "span %s σ=%.3f",
                     t.spans[i].name, t.spans[i].sigma);
            cos_v160_log_ring_push(&lr, COS_V160_LOG_INFO,
                t.spans[i].kernel, t.trace_id, t.spans[i].sigma,
                msg, t.spans[i].end_unix_nano);
        }
        cos_v160_log_ring_push(&lr, COS_V160_LOG_INFO, "v106",
            t.trace_id, t.sigma_product, "request end",
            t.spans[t.n_spans - 1].end_unix_nano);
        char out[16384];
        cos_v160_log_ring_to_ndjson(&lr, out, sizeof(out));
        fputs(out, stdout);
    }
    return 0;
}
