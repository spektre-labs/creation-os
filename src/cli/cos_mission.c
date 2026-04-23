/*
 * cos-mission — long-horizon σ-checkpointed execution CLI.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "../sigma/mission.h"
#include "../sigma/coherence_watchdog.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(FILE *fp)
{
    fputs(
        "cos-mission — σ-checkpointed missions\n"
        "  cos mission --goal \"…\"        create + run mission\n"
        "  cos mission --status            show persisted current mission\n"
        "  cos mission --resume            resume from mission.state\n"
        "  cos mission --abort             load current + report (abort marker)\n"
        "  cos mission --history           list mission IDs under ~/.cos/missions\n"
        "Environment: COS_MISSION_OFFLINE=1  deterministic self-test mode\n",
        fp);
}

static int current_mission_id(char *out, size_t cap)
{
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    char path[768];
    snprintf(path, sizeof(path), "%s/.cos/missions/current", home);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -2;
    if (!fgets(out, (int)cap, fp)) {
        fclose(fp);
        return -3;
    }
    fclose(fp);
    size_t L = strlen(out);
    while (L > 0 && (out[L - 1] == '\n' || out[L - 1] == '\r')) {
        out[L - 1] = '\0';
        --L;
    }
    return (L > 0) ? 0 : -4;
}

static int cmd_status(void)
{
    char id[COS_MISSION_ID_CHARS + 8];
    if (current_mission_id(id, sizeof(id)) != 0) {
        fputs("no current mission (missing ~/.cos/missions/current)\n", stdout);
        return 1;
    }
    char path[900];
    snprintf(path, sizeof(path), "%s/.cos/missions/%s/mission.state",
             getenv("HOME"), id);
    struct cos_mission m;
    if (cos_mission_load(&m, path) != 0) {
        printf("cannot load %s\n", path);
        return 2;
    }
    cos_mission_print_status(&m);
    return 0;
}

static int cmd_resume(void)
{
    char id[COS_MISSION_ID_CHARS + 8];
    if (current_mission_id(id, sizeof(id)) != 0)
        return 1;
    char path[900];
    snprintf(path, sizeof(path), "%s/.cos/missions/%s/mission.state",
             getenv("HOME"), id);
    struct cos_mission m;
    if (cos_mission_load(&m, path) != 0)
        return 2;
    cos_mission_resume(&m);
    cos_mission_run(&m);
    cos_mission_print_status(&m);
    return 0;
}

static int cmd_abort(void)
{
    char id[COS_MISSION_ID_CHARS + 8];
    if (current_mission_id(id, sizeof(id)) != 0)
        return 1;
    char path[900];
    snprintf(path, sizeof(path), "%s/.cos/missions/%s/mission.state",
             getenv("HOME"), id);
    struct cos_mission m;
    if (cos_mission_load(&m, path) != 0)
        return 2;
    cos_mission_pause(&m, "user abort (--abort)");
    char *r = cos_mission_report(&m);
    if (r)
        fputs(r, stdout);
    free(r);
    return 0;
}

static int cmd_history(void)
{
    const char *home = getenv("HOME");
    if (!home)
        return 1;
    char dir[768];
    snprintf(dir, sizeof(dir), "%s/.cos/missions", home);
    DIR *d = opendir(dir);
    if (!d) {
        perror(dir);
        return 2;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        if (strcmp(ent->d_name, "current") == 0)
            continue;
        printf("%s\n", ent->d_name);
    }
    closedir(d);
    return 0;
}

static int cmd_run_goal(const char *goal)
{
    struct cos_mission m;
    if (cos_mission_create(goal, &m) != 0) {
        fputs("cos-mission: create failed\n", stderr);
        return 3;
    }
    struct cos_cw_config wc;
    cos_cw_default_config(&wc);
    const char *wd = getenv("COS_MISSION_WATCHDOG");
    if (wd != NULL && wd[0] == '1')
        (void)cos_cw_start(&wc);
    cos_mission_run(&m);
    (void)cos_cw_stop();
    cos_mission_print_status(&m);
    return 0;
}

int cos_mission_main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--status") == 0)
            return cmd_status();
        if (strcmp(argv[i], "--resume") == 0)
            return cmd_resume();
        if (strcmp(argv[i], "--abort") == 0)
            return cmd_abort();
        if (strcmp(argv[i], "--history") == 0)
            return cmd_history();
        if (strcmp(argv[i], "--goal") == 0 && i + 1 < argc) {
            return cmd_run_goal(argv[i + 1]);
        }
        ++i;
    }
    usage(stderr);
    return 2;
}

#ifdef COS_MISSION_MAIN
int main(int argc, char **argv)
{
    return cos_mission_main(argc, argv);
}
#endif
