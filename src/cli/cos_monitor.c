/*
 * cos monitor — read ~/.cos/omega/events.jsonl; table, summary, sparkline, HTML.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_monitor.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct mon_evt {
    int       has_turn;
    int64_t   t_ms;
    int       turn;
    double    sigma;
    double    k_eff;
    int       attribution;
    char      action[24];
    int       cache_hit;
    double    energy_j;
    double    co2_g;
    int64_t   latency_ms;
    int       consciousness;
    int       skills;
    int       episodes;
    double    sigma_trend;
    double    green_score;
    char      green_grade;

    int       is_alert;
    char      alert[240];
    char      alert_level[20];
};

static int mon_argv_skip_monitor(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "monitor") == 0)
        return 2;
    return 1;
}

static int mon_events_path(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    const char *e = getenv("COS_OMEGA_EVENTS");
    if (e != NULL && e[0] != '\0')
        return snprintf(buf, cap, "%s", e);
    if (!h || !h[0])
        h = "/tmp";
    return snprintf(buf, cap, "%s/.cos/omega/events.jsonl", h);
}

static double mon_find_json_double(const char *ln, const char *key)
{
    char       pat[48];
    const char *p;
    snprintf(pat, sizeof pat, "\"%s\":", key);
    p = strstr(ln, pat);
    if (!p)
        return NAN;
    return strtod(p + strlen(pat), NULL);
}

static int64_t mon_find_json_llong(const char *ln, const char *key)
{
    char pat[48];
    const char *p;
    snprintf(pat, sizeof pat, "\"%s\":", key);
    p = strstr(ln, pat);
    if (!p)
        return 0;
    return (int64_t)strtoll(p + strlen(pat), NULL, 10);
}

static int mon_find_json_int(const char *ln, const char *key)
{
    return (int)mon_find_json_llong(ln, key);
}

static int mon_parse_cache_hit(const char *ln)
{
    const char *p = strstr(ln, "\"cache_hit\":");
    if (!p)
        return 0;
    p += 12;
    while (*p == ' ' || *p == '\t')
        ++p;
    return (strncmp(p, "true", 4) == 0);
}

static void mon_parse_action(const char *ln, char *out, size_t cap)
{
    const char *p = strstr(ln, "\"action\":\"");
    out[0] = '\0';
    if (!p || cap < 2)
        return;
    p += 11;
    {
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < cap)
            out[i++] = *p++;
        out[i] = '\0';
    }
}

static void mon_parse_line(const char *ln, struct mon_evt *ev)
{
    memset(ev, 0, sizeof(*ev));
    if (strstr(ln, "\"alert\"") != NULL && strstr(ln, "\"level\"") != NULL) {
        const char *pa, *pb, *qa, *eos;
        ev->is_alert = 1;
        ev->t_ms     = mon_find_json_llong(ln, "t");
        pa           = strstr(ln, "\"alert\":\"");
        if (pa) {
            pa += 10;
            qa = strchr(pa, '"');
            eos = qa ? qa : pa + strlen(pa);
            if (eos > pa && (size_t)(eos - pa) < sizeof ev->alert)
                snprintf(ev->alert, sizeof ev->alert, "%.*s",
                         (int)(eos - pa), pa);
        }
        pb = strstr(ln, "\"level\":\"");
        if (pb) {
            pb += 10;
            qa = strchr(pb, '"');
            eos = qa ? qa : pb + strlen(pb);
            if (eos > pb && (size_t)(eos - pb) < sizeof ev->alert_level)
                snprintf(ev->alert_level, sizeof ev->alert_level, "%.*s",
                         (int)(eos - pb), pb);
        }
        return;
    }

    ev->has_turn    = 1;
    ev->t_ms        = mon_find_json_llong(ln, "t");
    ev->turn        = mon_find_json_int(ln, "turn");
    ev->sigma       = mon_find_json_double(ln, "sigma");
    ev->k_eff       = mon_find_json_double(ln, "k_eff");
    ev->attribution = mon_find_json_int(ln, "attribution");
    mon_parse_action(ln, ev->action, sizeof ev->action);
    ev->cache_hit    = mon_parse_cache_hit(ln);
    ev->energy_j     = mon_find_json_double(ln, "energy_j");
    ev->co2_g        = mon_find_json_double(ln, "co2_g");
    ev->latency_ms   = mon_find_json_llong(ln, "latency_ms");
    ev->consciousness = mon_find_json_int(ln, "consciousness");
    ev->skills       = mon_find_json_int(ln, "skills");
    ev->episodes     = mon_find_json_int(ln, "episodes");
    ev->sigma_trend  = mon_find_json_double(ln, "sigma_trend");
    ev->green_score  = mon_find_json_double(ln, "green_score");
    {
        const char *p = strstr(ln, "\"green_grade\":\"");
        if (p) {
            p += 15;
            ev->green_grade = *p ? *p : '?';
        } else
            ev->green_grade = '?';
    }
}

static void mon_print_formatted(const struct mon_evt *ev)
{
    if (ev->is_alert) {
        printf("  [\xe2\x9a\xa0 %s] %s\n", ev->alert_level, ev->alert);
        return;
    }
    printf("  [turn %3d] σ=%.3f k=%.3f %-7s cache=%-3s %6lldms %.4fJ\n",
           ev->turn, ev->sigma, ev->k_eff, ev->action,
           ev->cache_hit ? "yes" : "no", (long long)ev->latency_ms,
           ev->energy_j);
}

static const char *mon_spark_utf8(double sigma01)
{
    static const char *blk[8] = {
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
        "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
    };
    int idx;
    if (!(sigma01 == sigma01) || sigma01 < 0.)
        sigma01 = 0.;
    if (sigma01 > 1.)
        sigma01 = 1.;
    idx = (int)(sigma01 * 7.999);
    if (idx < 0)
        idx = 0;
    if (idx > 7)
        idx = 7;
    return blk[idx];
}

static int mon_process_file(FILE *fp, void *ctx,
                            void (*fn)(const struct mon_evt *, void *))
{
    char   buf[8192];
    while (fgets(buf, sizeof buf, fp) != NULL) {
        struct mon_evt ev;
        mon_parse_line(buf, &ev);
        fn(&ev, ctx);
    }
    return 0;
}

struct mon_agg {
    int      n_turn;
    double   sum_sigma;
    double   sum_k;
    double   sum_e;
    double   sum_co2;
    int      cache_hits;
    int      first_con;
    int      last_con;
    int      first_con_set;
    double   last_sigma_trend;
    char     last_grade;
    double   last_green;
};

static void mon_agg_cb(const struct mon_evt *ev, void *ctx)
{
    struct mon_agg *a = (struct mon_agg *)ctx;
    if (ev->is_alert || !ev->has_turn)
        return;
    a->n_turn++;
    if (!isnan(ev->sigma))
        a->sum_sigma += ev->sigma;
    if (!isnan(ev->k_eff))
        a->sum_k += ev->k_eff;
    a->sum_e += ev->energy_j;
    a->sum_co2 += ev->co2_g;
    if (ev->cache_hit)
        a->cache_hits++;
    if (!isnan(ev->sigma_trend))
        a->last_sigma_trend = ev->sigma_trend;
    if (ev->green_grade && ev->green_grade != '?') {
        a->last_grade = ev->green_grade;
        a->last_green = ev->green_score;
    }
    if (!a->first_con_set) {
        a->first_con    = ev->consciousness;
        a->first_con_set = 1;
    }
    a->last_con = ev->consciousness;
}

static void mon_run_summary(FILE *fp)
{
    struct mon_agg a;
    memset(&a, 0, sizeof a);
    rewind(fp);
    mon_process_file(fp, &a, mon_agg_cb);
    if (a.n_turn <= 0) {
        fputs("cos monitor: no turn rows in events file\n", stderr);
        return;
    }
    printf("Turns: %d | σ̄=%.3f | k̄=%.3f | Cache: %.0f%%\n", a.n_turn,
           a.sum_sigma / (double)a.n_turn, a.sum_k / (double)a.n_turn,
           100.0 * (double)a.cache_hits / (double)a.n_turn);
    printf("Energy: %.2fJ | CO2: %.4fg | Grade: %c\n", a.sum_e, a.sum_co2,
           a.last_grade ? a.last_grade : '?');
    printf("σ trend (last): %.4f/turn\n", a.last_sigma_trend);
    printf("Consciousness: Level %d → Level %d (%s)\n", a.first_con,
           a.last_con,
           a.last_con > a.first_con ? "rising" :
           a.last_con < a.first_con ? "falling" : "stable");
}

struct mon_sigma_ctx {
    double sigmas[4096];
    int    n;
};

static void mon_sigma_collect(const struct mon_evt *ev, void *ctx)
{
    struct mon_sigma_ctx *s = (struct mon_sigma_ctx *)ctx;
    if (ev->is_alert || !ev->has_turn)
        return;
    if (s->n < 4096 && !isnan(ev->sigma))
        s->sigmas[s->n++] = ev->sigma;
}

static void mon_run_sigma(FILE *fp)
{
    struct mon_sigma_ctx s;
    memset(&s, 0, sizeof s);
    rewind(fp);
    mon_process_file(fp, &s, mon_sigma_collect);
    if (s.n <= 0) {
        fputs("(no σ data)\n", stdout);
        return;
    }
    fputs("σ: ", stdout);
    {
        int i;
        for (i = 0; i < s.n; ++i)
            fputs(mon_spark_utf8(s.sigmas[i]), stdout);
    }
    fputs(" ", stdout);
    if (s.n >= 2) {
        double d = s.sigmas[s.n - 1] - s.sigmas[0];
        if (d < -1e-6)
            fputs("(declining ↓)\n", stdout);
        else if (d > 1e-6)
            fputs("(rising ↑)\n", stdout);
        else
            fputs("(flat)\n", stdout);
    } else
        fputc('\n', stdout);
}

static void mon_run_csv(FILE *fp)
{
    fputs("t,turn,sigma,k_eff,attribution,action,cache_hit,energy_j,co2_g,"
          "latency_ms,consciousness,skills,episodes,sigma_trend,green_score,"
          "green_grade\n",
          stdout);
    rewind(fp);
    {
        char buf[8192];
        while (fgets(buf, sizeof buf, fp) != NULL) {
            struct mon_evt ev;
            mon_parse_line(buf, &ev);
            if (ev.is_alert || !ev.has_turn)
                continue;
            printf("%lld,%d,%.6f,%.6f,%d,%s,%s,%.9f,%.9f,%lld,%d,%d,%d,%.6f,"
                   "%.4f,%c\n",
                   (long long)ev.t_ms, ev.turn, ev.sigma, ev.k_eff,
                   ev.attribution, ev.action, ev.cache_hit ? "true" : "false",
                   ev.energy_j, ev.co2_g, (long long)ev.latency_ms,
                   ev.consciousness, ev.skills, ev.episodes, ev.sigma_trend,
                   ev.green_score, ev.green_grade ? ev.green_grade : '?');
        }
    }
}

static void mon_run_plot(FILE *fp)
{
    fputs("# turn sigma k_eff\n", stdout);
    rewind(fp);
    {
        char buf[8192];
        while (fgets(buf, sizeof buf, fp) != NULL) {
            struct mon_evt ev;
            mon_parse_line(buf, &ev);
            if (ev.is_alert || !ev.has_turn)
                continue;
            printf("%d %.6f %.6f\n", ev.turn, ev.sigma, ev.k_eff);
        }
    }
}

static void mon_emit_html(FILE *in, FILE *out)
{
    fputs("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
          "<title>Creation OS — Ω monitor</title>\n<style>"
          "body{font-family:system-ui,Segoe UI,sans-serif;margin:16px;"
          "background:#0d1117;color:#e6edf3}"
          "h1{font-size:1.1rem}canvas{border:1px solid #30363d;"
          "background:#010409;margin:8px 0}#meta{font-size:0.85rem;"
          "color:#8b949e}</style></head><body>\n",
          out);
    fputs("<h1>Ω telemetry (embedded)</h1><p id=\"meta\"></p>\n", out);
    fputs("<canvas id=\"c1\" width=\"720\" height=\"160\"></canvas>\n", out);
    fputs("<canvas id=\"c2\" width=\"720\" height=\"160\"></canvas>\n", out);
    fputs("<script>\nconst EV=[];\n", out);
    rewind(in);
    {
        char buf[8192];
        while (fgets(buf, sizeof buf, in) != NULL) {
            struct mon_evt ev;
            mon_parse_line(buf, &ev);
            if (ev.is_alert || !ev.has_turn)
                continue;
            fprintf(out,
                    "EV.push({t:%lld,turn:%d,sigma:%.6f,k_eff:%.6f,"
                    "energy_j:%.9f,cache_hit:%s,consciousness:%d,"
                    "green_score:%.4f});\n",
                    (long long)ev.t_ms, ev.turn, ev.sigma, ev.k_eff,
                    ev.energy_j, ev.cache_hit ? "true" : "false",
                    ev.consciousness, ev.green_score);
        }
    }
    fputs(
        "function lineChart(cv,getY){const c=document.getElementById(cv);"
        "const x=c.getContext('2d');const w=c.width,h=c.height;"
        "x.fillStyle='#161b22';x.fillRect(0,0,w,h);"
        "if(!EV.length)return;let mn=1,mx=0;for(const e of EV){const "
        "v=getY(e);if(v<mn)mn=v;if(v>mx)mx=v;}if(mx<=mn){mx=mn+1e-6;}"
        "x.strokeStyle='#58a6ff';x.beginPath();EV.forEach((e,i)=>{const "
        "px=i/(EV.length-1||1)*(w-4)+2;const py=h-2-((getY(e)-mn)/(mx-mn)*(h-4));"
        "if(i)x.lineTo(px,py);else x.moveTo(px,py);});x.stroke();}"
        "lineChart('c1',e=>e.sigma);lineChart('c2',e=>e.k_eff);"
        "const el=document.getElementById('meta');"
        "if(EV.length)el.textContent='Turns: '+EV.length+' | last σ='"
        "+EV[EV.length-1].sigma.toFixed(3);"
        "</script></body></html>\n",
        out);
}

static int mon_run_html(const char *path)
{
    FILE *in = fopen(path, "r");
    if (!in) {
        fprintf(stderr, "cos monitor: cannot read %s: %s\n", path,
                strerror(errno));
        return 1;
    }
    mon_emit_html(in, stdout);
    fclose(in);
    return 0;
}

static void mon_run_default(FILE *fp)
{
    rewind(fp);
    {
        char buf[8192];
        while (fgets(buf, sizeof buf, fp) != NULL) {
            struct mon_evt ev;
            mon_parse_line(buf, &ev);
            if (ev.has_turn || ev.is_alert)
                mon_print_formatted(&ev);
        }
    }
}

static int mon_follow_path(const char *path)
{
    FILE *fp;
    long   pos = 0;
    fp         = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "cos monitor: %s: %s\n", path, strerror(errno));
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    for (;;) {
        long npos;
        clearerr(fp);
        fseek(fp, pos, SEEK_SET);
        {
            char buf[8192];
            while (fgets(buf, sizeof buf, fp) != NULL) {
                struct mon_evt ev;
                mon_parse_line(buf, &ev);
                if (ev.has_turn || ev.is_alert)
                    mon_print_formatted(&ev);
                fflush(stdout);
            }
        }
        npos = ftell(fp);
        if (npos >= pos)
            pos = npos;
        usleep(250000);
        /* reopen if truncated */
        if (ferror(fp)) {
            fclose(fp);
            fp = fopen(path, "r");
            if (!fp)
                return 1;
            pos = 0;
        }
    }
}

int cos_monitor_main(int argc, char **argv)
{
    char  path[768];
    FILE *fp;
    int   base = mon_argv_skip_monitor(argc, argv);
    int   i;
    int   mode_default = 1;
    int   mode_csv     = 0;
    int   mode_summary = 0;
    int   mode_sigma   = 0;
    int   mode_plot    = 0;
    int   mode_html    = 0;
    int   mode_follow  = 0;
    const char *path_arg = NULL;

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--csv") == 0)
            mode_csv = 1, mode_default = 0;
        else if (strcmp(argv[i], "--summary") == 0)
            mode_summary = 1, mode_default = 0;
        else if (strcmp(argv[i], "--sigma") == 0)
            mode_sigma = 1, mode_default = 0;
        else if (strcmp(argv[i], "--plot") == 0)
            mode_plot = 1, mode_default = 0;
        else if (strcmp(argv[i], "--html") == 0)
            mode_html = 1, mode_default = 0;
        else if (strcmp(argv[i], "--follow") == 0 ||
                 strcmp(argv[i], "-f") == 0)
            mode_follow = 1;
        else if (argv[i][0] != '-')
            path_arg = argv[i];
    }

    if (path_arg)
        snprintf(path, sizeof path, "%s", path_arg);
    else if (mon_events_path(path, sizeof path) >= (int)sizeof path) {
        fputs("cos monitor: events path too long\n", stderr);
        return 2;
    }

    if (mode_follow && !path_arg)
        return mon_follow_path(path);

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "cos monitor: cannot open %s: %s\n", path,
                strerror(errno));
        return 1;
    }

    if (mode_html) {
        fclose(fp);
        return mon_run_html(path_arg ? path_arg : path);
    }

    if (mode_csv)
        mon_run_csv(fp);
    else if (mode_summary)
        mon_run_summary(fp);
    else if (mode_sigma)
        mon_run_sigma(fp);
    else if (mode_plot)
        mon_run_plot(fp);
    else
        mon_run_default(fp);

    fclose(fp);
    (void)mode_default;
    return 0;
}

#if defined(COS_MONITOR_MAIN)
int main(int argc, char **argv)
{
    return cos_monitor_main(argc, argv);
}
#endif
