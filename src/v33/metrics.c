/* SPDX-License-Identifier: AGPL-3.0-or-later */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "metrics.h"

#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <sys/time.h>
#endif

static int bin_for_sigma(float v)
{
    if (v < 0.f)
        v = 0.f;
    if (v > 1.f)
        v = 1.f;
    int b = (int)(v * (float)COS_METRICS_SIGMA_BINS);
    if (b >= COS_METRICS_SIGMA_BINS)
        b = COS_METRICS_SIGMA_BINS - 1;
    if (b < 0)
        b = 0;
    return b;
}

int cos_metrics_session_open(cos_metrics_session_t *m, const char *metrics_dir)
{
    if (!m)
        return -1;
    memset(m, 0, sizeof(*m));
    const char *dir = metrics_dir && *metrics_dir ? metrics_dir : "metrics";
    struct timespec ts;
    memset(&ts, 0, sizeof ts);
#if defined(CLOCK_REALTIME)
    (void)clock_gettime(CLOCK_REALTIME, &ts);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = (long)tv.tv_usec * 1000L;
#endif
    if (snprintf(m->path, sizeof m->path, "%s/session_%lld_%09ld.jsonl", dir, (long long)ts.tv_sec,
            (long)ts.tv_nsec)
        >= (int)sizeof m->path)
        return -1;
    m->fp = fopen(m->path, "a");
    if (!m->fp)
        return -1;
    return 0;
}

void cos_metrics_session_close(cos_metrics_session_t *m)
{
    if (!m)
        return;
    if (m->fp) {
        fclose(m->fp);
        m->fp = NULL;
    }
}

void cos_metrics_event_json(cos_metrics_session_t *m, const char *json_line)
{
    if (!m || !m->fp || !json_line)
        return;
    fputs(json_line, m->fp);
    if (json_line[0] && json_line[strlen(json_line) - 1] != '\n')
        fputc('\n', m->fp);
    fflush(m->fp);
}

void cos_metrics_task_started(cos_metrics_session_t *m, int task_id)
{
    if (!m)
        return;
    m->task_started++;
    char buf[256];
    (void)snprintf(buf, sizeof buf, "{\"event\":\"task_started\",\"task_id\":%d}", task_id);
    cos_metrics_event_json(m, buf);
}

void cos_metrics_task_outcome(cos_metrics_session_t *m, int task_id, const char *outcome)
{
    if (!m || !outcome)
        return;
    if (!strcmp(outcome, "completed"))
        m->task_completed++;
    else if (!strcmp(outcome, "failed"))
        m->task_failed++;
    else if (!strcmp(outcome, "abstained"))
        m->task_abstained++;
    char buf[320];
    (void)snprintf(buf, sizeof buf,
        "{\"event\":\"task_outcome\",\"task_id\":%d,\"outcome\":\"%s\"}", task_id, outcome);
    cos_metrics_event_json(m, buf);
}

void cos_metrics_schema(cos_metrics_session_t *m, int valid)
{
    if (!m)
        return;
    if (valid)
        m->schema_valid++;
    else
        m->schema_invalid++;
}

void cos_metrics_tool_exec(cos_metrics_session_t *m, int ok)
{
    if (!m)
        return;
    if (ok)
        m->tool_exec_ok++;
    else
        m->tool_exec_fail++;
}

void cos_metrics_latency_e2e_ns(cos_metrics_session_t *m, uint64_t ns)
{
    if (!m)
        return;
    m->e2e_ns_sum += ns;
    m->e2e_count++;
}

void cos_metrics_latency_token_ns(cos_metrics_session_t *m, uint64_t ns)
{
    if (!m)
        return;
    m->tok_ns_sum += ns;
    m->tok_count++;
}

void cos_metrics_sigma_observe(cos_metrics_session_t *m, int channel_index, float sigma_0_1)
{
    if (!m)
        return;
    if (channel_index < 0 || channel_index >= COS_METRICS_SIGMA_CHANNELS)
        return;
    int b = bin_for_sigma(sigma_0_1);
    m->sigma_hist[channel_index][b]++;
}
