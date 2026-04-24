/*
 * cos life — personal daemon (orchestration: autonomy, learn, sovereign).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_life.h"

#include "../sigma/learn_engine.h"
#include "../sigma/sovereign_limits.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <strings.h>
#include <unistd.h>

static struct cos_life_state *g_st;
static sqlite3               *g_db;
static volatile sig_atomic_t   g_stop;

static int64_t wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000LL;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static const char *life_home(void)
{
    const char *h = getenv("HOME");
    return (h != NULL && h[0] != '\0') ? h : ".";
}

static int life_mkdir_p(const char *dir)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", dir);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (tmp[0] != '\0')
                (void)mkdir(tmp, 0700);
            *p = '/';
        }
    }
    return mkdir(tmp, 0700);
}

static int life_paths(char *dbpath, size_t dbcap, char *repdir, size_t repcap)
{
    const char *h = life_home();
    if (snprintf(dbpath, dbcap, "%s/.cos/life/tasks.db", h) >= (int)dbcap)
        return -1;
    if (snprintf(repdir, repcap, "%s/.cos/life/reports", h) >= (int)repcap)
        return -1;
    return 0;
}

static int life_exec_sql(const char *sql)
{
    char *err = NULL;
    int   rc  = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    if (err != NULL)
        sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

static uint64_t life_fnv1a64(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    uint64_t               h = 14695981039346656037ULL;
    while (p != NULL && *p != '\0') {
        h ^= (uint64_t)*p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static int life_schema(void)
{
    return life_exec_sql(
        "CREATE TABLE IF NOT EXISTS tasks(\n"
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "  description TEXT NOT NULL,\n"
        "  priority INTEGER NOT NULL,\n"
        "  status INTEGER NOT NULL,\n"
        "  sigma REAL NOT NULL DEFAULT 0.5,\n"
        "  due_ms INTEGER NOT NULL DEFAULT 0,\n"
        "  created_ms INTEGER NOT NULL,\n"
        "  completed_ms INTEGER NOT NULL DEFAULT 0,\n"
        "  result TEXT);\n"
        "CREATE INDEX IF NOT EXISTS idx_tasks_open ON tasks(status,"
        " priority, due_ms, created_ms);\n");
}

static void life_row_to_task(sqlite3_stmt *st, struct cos_life_task *t)
{
    memset(t, 0, sizeof(*t));
    {
        const unsigned char *d = sqlite3_column_text(st, 1);
        if (d != NULL)
            snprintf(t->description, sizeof t->description, "%s", (const char *)d);
    }
    t->priority  = sqlite3_column_int(st, 2);
    t->status    = sqlite3_column_int(st, 3);
    t->sigma     = (float)sqlite3_column_double(st, 4);
    t->due_ms    = sqlite3_column_int64(st, 5);
    t->created_ms = sqlite3_column_int64(st, 6);
    {
        const unsigned char *r = sqlite3_column_text(st, 8);
        if (r != NULL)
            snprintf(t->result, sizeof t->result, "%s", (const char *)r);
    }
}

static int life_refresh(struct cos_life_state *st)
{
    sqlite3_stmt *s = NULL;
    const char   *q =
        "SELECT id,description,priority,status,sigma,due_ms,created_ms,"
        "completed_ms,result FROM tasks ORDER BY status ASC, priority ASC,"
        " due_ms ASC, created_ms ASC LIMIT 64;";
    int           i, n_done = 0, n_all = 0;

    st->n_tasks = 0;
    if (g_db == NULL)
        return -1;
    if (sqlite3_prepare_v2(g_db, q, -1, &s, NULL) != SQLITE_OK)
        return -2;
    i = 0;
    while (sqlite3_step(s) == SQLITE_ROW && i < 64) {
        life_row_to_task(s, &st->tasks[i]);
        i++;
    }
    sqlite3_finalize(s);
    st->n_tasks = i;

    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM tasks;", -1, &s,
                           NULL)
        == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW)
            n_all = sqlite3_column_int(s, 0);
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(
            g_db, "SELECT COUNT(*) FROM tasks WHERE status=2;", -1, &s,
            NULL)
        == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW)
            n_done = sqlite3_column_int(s, 0);
        sqlite3_finalize(s);
    }
    st->productivity_score =
        n_all > 0 ? (float)n_done / (float)n_all : 0.f;
    return 0;
}

int cos_life_init(struct cos_life_state *state)
{
    char dbpath[512], repdir[512];

    memset(state, 0, sizeof(*state));
    g_st = state;
    state->started_ms = wall_ms();
    state->mode         = COS_LIFE_MODE_SLEEPING;

    if (life_paths(dbpath, sizeof dbpath, repdir, sizeof repdir) != 0)
        return -1;
    if (life_mkdir_p(repdir) != 0 && errno != EEXIST)
        return -2;
    {
        char dirc[512];
        snprintf(dirc, sizeof dirc, "%s", dbpath);
        char *slash = strrchr(dirc, '/');
        if (slash != NULL) {
            *slash = '\0';
            (void)mkdir(dirc, 0700);
        }
    }
    if (g_db != NULL)
        sqlite3_close(g_db);
    g_db = NULL;
    if (sqlite3_open_v2(dbpath, &g_db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)
        != SQLITE_OK)
        return -3;
    if (life_schema() != 0)
        return -4;
    if (cos_sovereign_init(NULL) != 0)
        return -5;
    (void)cos_learn_init();
    if (cos_autonomy_init(&state->autonomy) != 0)
        return -6;
    cos_autonomy_configure(1);
    cos_autonomy_bind_for_introspect(&state->autonomy);
    return life_refresh(state);
}

int cos_life_add_task(const char *description, int priority, int64_t due_ms)
{
    sqlite3_stmt *st = NULL;
    const char   *ins =
        "INSERT INTO tasks(description,priority,status,sigma,due_ms,"
        "created_ms,completed_ms,result) VALUES(?,?,0,0.5,?,?,0,'');";
    int64_t       now = wall_ms();

    if (g_db == NULL || g_st == NULL || description == NULL
        || description[0] == '\0')
        return -1;
    if (priority < 1)
        priority = 3;
    if (priority > 3)
        priority = 1;
    if (sqlite3_prepare_v2(g_db, ins, -1, &st, NULL) != SQLITE_OK)
        return -2;
    sqlite3_bind_text(st, 1, description, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, priority);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)due_ms);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)now);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return -3;
    }
    sqlite3_finalize(st);
    return life_refresh(g_st);
}

static int life_update_task_done(int64_t rowid, float sigma,
                                  const char *result)
{
    sqlite3_stmt *st = NULL;
    const char   *up =
        "UPDATE tasks SET status=2, sigma=?, completed_ms=?, result=? "
        "WHERE id=?;";
    int64_t       now = wall_ms();

    if (sqlite3_prepare_v2(g_db, up, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_double(st, 1, (double)sigma);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)now);
    sqlite3_bind_text(st, 3, result != NULL ? result : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)rowid);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return -2;
    }
    sqlite3_finalize(st);
    return 0;
}

static int life_update_task_status(int64_t rowid, int status)
{
    sqlite3_stmt *st = NULL;
    const char   *up = "UPDATE tasks SET status=? WHERE id=?;";
    if (sqlite3_prepare_v2(g_db, up, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, status);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)rowid);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return -2;
    }
    sqlite3_finalize(st);
    return 0;
}

static int life_write_report_md(const char *slug, const char *body)
{
    char          repdir[512], path[768], tmp[64];
    struct tm    *tmv;
    time_t         t;
    FILE          *fp;
    struct cos_sovereign_state sv;

    if (slug == NULL || body == NULL)
        return -1;
    memset(&sv, 0, sizeof sv);
    if (cos_sovereign_check(&sv, COS_SOVEREIGN_ACTION_FILE_WRITE) != 0)
        return -2;
    if (life_paths(tmp, sizeof tmp, repdir, sizeof repdir) != 0)
        return -3;
    if (life_mkdir_p(repdir) != 0 && errno != EEXIST)
        return -4;
    t   = time(NULL);
    tmv = localtime(&t);
    if (tmv == NULL)
        return -5;
    if (snprintf(path, sizeof path, "%s/%04d-%02d-%02d_%s.md", repdir,
                 tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday, slug)
        >= (int)sizeof path)
        return -6;
    fp = fopen(path, "w");
    if (fp == NULL)
        return -7;
    fprintf(fp, "# cos life report\n\n%s\n", body);
    fclose(fp);
    return 0;
}

static void life_slug(const char *desc, char *out, size_t cap)
{
    size_t i, j = 0;
    for (i = 0; desc[i] != '\0' && j + 1 < cap; ++i) {
        unsigned char c = (unsigned char)desc[i];
        if (isalnum(c) && j < 48) {
            out[j++] = (char)tolower((int)c);
        } else if (j > 0 && out[j - 1] != '_') {
            out[j++] = '_';
        }
    }
    while (j > 0 && out[j - 1] == '_')
        j--;
    if (j == 0) {
        out[0] = 'x';
        j      = 1;
    }
    out[j] = '\0';
}

int cos_life_run_cycle(struct cos_life_state *state)
{
    sqlite3_stmt *st = NULL;
    const char   *sel =
        "SELECT id,description,priority,status,sigma,due_ms,created_ms,"
        "completed_ms,result FROM tasks WHERE status=0 AND description "
        "LIKE '[research]%' ORDER BY priority ASC, due_ms ASC, "
        "created_ms ASC LIMIT 1;";
    struct cos_sovereign_state sv;
    int64_t                    rid;
    const char                *desc;
    char                       topic[256];
    struct cos_learn_task      lt;
    struct cos_learn_result     lr;
    char                        repbody[4096];
    char                        slug[96];

    if (g_db == NULL || state == NULL)
        return -1;

    /* User interrupt: touch ~/.cos/life/interrupt to skip heavy work once */
    {
        char ip[512];
        snprintf(ip, sizeof ip, "%s/.cos/life/interrupt", life_home());
        if (access(ip, F_OK) == 0) {
            (void)unlink(ip);
            state->mode = COS_LIFE_MODE_WAITING;
            snprintf(state->daily_summary, sizeof state->daily_summary,
                     "Interrupted by user signal file; skipping cycle.\n");
            return 0;
        }
    }

    state->uptime_ms = wall_ms() - state->started_ms;
    memset(&sv, 0, sizeof sv);
    cos_autonomy_omega_tick(&state->autonomy, NULL, 0.35f, "cos_life", "");

    if (sqlite3_prepare_v2(g_db, sel, -1, &st, NULL) != SQLITE_OK)
        return -2;
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        state->mode = COS_LIFE_MODE_SLEEPING;
        return 0;
    }
    rid  = sqlite3_column_int64(st, 0);
    desc = (const char *)sqlite3_column_text(st, 1);
    sqlite3_finalize(st);

    state->mode = COS_LIFE_MODE_WORKING;

    if (desc != NULL && strncmp(desc, "[research]", 10) == 0) {
        memset(&sv, 0, sizeof sv);
        if (cos_sovereign_check(&sv, COS_SOVEREIGN_ACTION_NETWORK) != 0) {
            (void)life_update_task_status(rid, 3);
            snprintf(repbody, sizeof repbody,
                     "Blocked: sovereign brake on network (%s)",
                     sv.last_brake_reason[0] ? sv.last_brake_reason
                                            : "limit");
            (void)life_update_task_done(rid, 0.99f, repbody);
            state->autonomy.goals_failed++;
            state->mode = COS_LIFE_MODE_WAITING;
            goto refresh_only;
        }
        state->mode = COS_LIFE_MODE_LEARNING;
        snprintf(topic, sizeof topic, "%s", desc + 10);
        while (topic[0] == ' ')
            memmove(topic, topic + 1, strlen(topic));
        memset(&lt, 0, sizeof lt);
        lt.domain_hash   = life_fnv1a64(topic);
        lt.current_sigma = 0.72f;
        lt.target_sigma  = 0.28f;
        lt.priority      = 1;
        snprintf(lt.topic, sizeof lt.topic, "%.250s", topic);

        if (cos_learn_research(&lt, &lr) != 0) {
            (void)life_update_task_status(rid, 3);
            (void)life_update_task_done(rid, 0.95f, "learn_research failed");
            state->autonomy.goals_failed++;
        } else {
            float sigma_use = lr.sigma_after;
            if (sigma_use > 0.78f) {
                /* High σ: record only, no extra side effects (MCP would gate here). */
                snprintf(repbody, sizeof repbody,
                         "Summary (high σ=%.2f, not auto-acting on MCP):\n%s\n"
                         "Source: %s\nVerified: %d\n",
                         (double)sigma_use, lr.summary, lr.source_url,
                         lr.verified);
            } else {
                (void)cos_learn_store(&lr);
                cos_autonomy_note_learn(&state->autonomy, lr.domain_hash,
                                        lr.sigma_before, lr.sigma_after);
                snprintf(repbody, sizeof repbody,
                         "σ before=%.2f after=%.2f verified=%d\n%s\nSource: %s\n",
                         (double)lr.sigma_before, (double)lr.sigma_after,
                         lr.verified, lr.summary, lr.source_url);
            }
            (void)life_update_task_done(rid, sigma_use, repbody);
            life_slug(topic, slug, sizeof slug);
            (void)life_write_report_md(slug, repbody);
            state->autonomy.goals_completed++;
        }
    }

refresh_only:
    return life_refresh(state);
}

int cos_life_daily_summary(char *summary, int max_len)
{
    int i, n;
    if (summary == NULL || max_len <= 0 || g_st == NULL)
        return -1;
    (void)life_refresh(g_st);
    n = snprintf(summary, (size_t)max_len,
                 "tasks loaded=%d productivity=%.2f uptime=%lld ms\n",
                 g_st->n_tasks, (double)g_st->productivity_score,
                 (long long)g_st->uptime_ms);
    if (n < 0 || n >= max_len)
        return -2;
    for (i = 0; i < g_st->n_tasks && n < max_len - 2; ++i) {
        int k = snprintf(summary + n, (size_t)(max_len - n),
                         " - [%d] %s σ=%.2f\n", g_st->tasks[i].status,
                         g_st->tasks[i].description,
                         (double)g_st->tasks[i].sigma);
        if (k < 0 || n + k >= max_len)
            break;
        n += k;
    }
    return 0;
}

int cos_life_report_overnight(char *report, int max_len)
{
    char   repdir[512], pat[512];
    DIR   *d;
    struct dirent *e;
    int    n = 0;

    if (report == NULL || max_len <= 0)
        return -1;
    report[0] = '\0';
    if (life_paths(pat, sizeof pat, repdir, sizeof repdir) != 0)
        return -2;
    d = opendir(repdir);
    if (d == NULL) {
        snprintf(report, (size_t)max_len, "(no reports dir yet)\n");
        return 0;
    }
    while ((e = readdir(d)) != NULL && n < max_len - 80) {
        size_t L;
        if (e->d_name[0] == '.')
            continue;
        L = strlen(e->d_name);
        if (L < 4 || strcmp(e->d_name + L - 3, ".md") != 0)
            continue;
        n += snprintf(report + n, (size_t)(max_len - n), " - %s\n",
                      e->d_name);
    }
    closedir(d);
    return 0;
}

static int usage(FILE *fp)
{
    fputs(
        "cos life — personal daemon (local tasks + learn + reports)\n"
        "  cos life                  run cycles until SIGINT or ~/.cos/life/halt\n"
        "  cos life --status         task counts + autonomy snapshot\n"
        "  cos life --today          text summary from SQLite tasks\n"
        "  cos life --morning        morning briefing (ASCII)\n"
        "  cos life --goal \"...\"     queue a goal (priority important)\n"
        "  cos life --remind \"...\" [--when tomorrow]\n"
        "  cos life --research \"...\"  queue [research] task (web + σ via learn)\n"
        "\n"
        "Data: ~/.cos/life/tasks.db  reports: ~/.cos/life/reports/*.md\n"
        "MCP: use `cos mcp` for file/shell/web; cos life only records when σ\n"
        "     is low enough for auto side-effects (research stores on success).\n"
        "Ω-loop: run `cos omega` separately for continuous perception stack.\n",
        fp);
    return 0;
}

int cos_life_main(int argc, char **argv)
{
    struct cos_life_state st;
    int                    i, rc = 0;
    const char            *goal = NULL, *rem = NULL, *when = NULL,
        *research = NULL;
    int do_status = 0, do_today = 0, do_morning = 0, daemon = 0;
    int                    j = (argc >= 2 && strcmp(argv[1], "life") == 0)
                 ? 2
                 : 1;

    if (argc <= j)
        daemon = 1;
    else {
        for (i = j; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
                return usage(stdout);
            if (strcmp(argv[i], "--status") == 0)
                do_status = 1;
            else if (strcmp(argv[i], "--today") == 0)
                do_today = 1;
            else if (strcmp(argv[i], "--morning") == 0)
                do_morning = 1;
            else if (strcmp(argv[i], "--goal") == 0 && i + 1 < argc)
                goal = argv[++i];
            else if (strcmp(argv[i], "--remind") == 0 && i + 1 < argc)
                rem = argv[++i];
            else if (strcmp(argv[i], "--when") == 0 && i + 1 < argc)
                when = argv[++i];
            else if (strcmp(argv[i], "--research") == 0 && i + 1 < argc)
                research = argv[++i];
            else {
                fprintf(stderr, "cos life: unknown arg %s\n", argv[i]);
                usage(stderr);
                return 2;
            }
        }
        daemon = 0;
    }

    if (cos_life_init(&st) != 0) {
        fprintf(stderr, "cos life: init failed\n");
        return 1;
    }

    if (goal != NULL) {
        if (cos_life_add_task(goal, 2, 0) != 0)
            return 3;
        printf("queued goal (priority important)\n");
    }
    if (rem != NULL) {
        int64_t due = 0;
        if (when != NULL && strcasecmp(when, "tomorrow") == 0)
            due = wall_ms() + 86400000LL;
        if (cos_life_add_task(rem, 1, due) != 0)
            return 4;
        printf("queued reminder (priority urgent)\n");
    }
    if (research != NULL) {
        char buf[600];
        snprintf(buf, sizeof buf, "[research] %.500s", research);
        if (cos_life_add_task(buf, 2, 0) != 0)
            return 5;
        printf("queued research task\n");
    }

    (void)life_refresh(&st);

    if (do_status) {
        printf("cos life: tasks=%d mode=%d productivity=%.2f uptime_ms=%lld\n",
               st.n_tasks, st.mode, (double)st.productivity_score,
               (long long)st.uptime_ms);
        cos_autonomy_report(&st.autonomy);
        return 0;
    }
    if (do_today) {
        char sum[4096];
        if (cos_life_daily_summary(sum, (int)sizeof sum) == 0)
            fputs(sum, stdout);
        return 0;
    }
    if (do_morning) {
        char rep[2048];
        int  n_open = 0, n_done = 0, k;
        (void)life_refresh(&st);
        for (k = 0; k < st.n_tasks; ++k) {
            if (st.tasks[k].status == 2)
                n_done++;
            else
                n_open++;
        }
        cos_life_report_overnight(rep, (int)sizeof rep);
        fputs("+-- Good morning (cos life) ---------------------------+\n",
              stdout);
        fputs("| Overnight:                                         |\n",
              stdout);
        printf("|   tasks open: %-4d   done (loaded window): %-4d      |\n",
               n_open, n_done);
        fputs("|   report files:                                    |\n", stdout);
        fputs(rep, stdout);
        fputs("| Today (first open items):                         |\n", stdout);
        for (k = 0; k < st.n_tasks && k < 8; ++k) {
            if (st.tasks[k].status == 2)
                continue;
            printf("|   %d. [P%d] %.52s |\n", k + 1, st.tasks[k].priority,
                   st.tasks[k].description);
        }
        fputs("| System:                                            |\n", stdout);
        printf("|   autonomy mode: %d  (use cos introspect for detail) |\n",
               st.autonomy.mode);
        fputs("| MCP: cos mcp (σ-gate in pipeline; high-σ = report) |\n", stdout);
        fputs("+----------------------------------------------------+\n", stdout);
        return 0;
    }

    if (goal != NULL || rem != NULL || research != NULL)
        return 0;
    if (!daemon) {
        usage(stdout);
        return 0;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    printf("cos life: daemon start (SIGINT or ~/.cos/life/halt to stop)\n");
    fflush(stdout);
    while (!g_stop) {
        char hpath[512];
        snprintf(hpath, sizeof hpath, "%s/.cos/life/halt", life_home());
        if (access(hpath, F_OK) == 0) {
            printf("cos life: halt file present, exiting\n");
            break;
        }
        if (cos_life_run_cycle(&st) != 0)
            fprintf(stderr, "cos life: cycle error\n");
        sleep(30);
    }
    cos_learn_shutdown();
    if (g_db != NULL) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    return rc;
}
