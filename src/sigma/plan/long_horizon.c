/*
 * Long-horizon planning — implementation (HORIZON-3).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "long_horizon.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int ensure_dir(const char *path) {
    if (mkdir(path, 0700) == 0) return 0;
    return errno == EEXIST ? 0 : -1;
}

static int write_snap(const char *root, int snap_1based, const char *desc,
                      char *out_path, size_t out_cap) {
    char dir[768];
    snprintf(dir, sizeof dir, "%s/snap_%d", root, snap_1based);
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) return -1;
    char mf[832];
    snprintf(mf, sizeof mf, "%s/MANIFEST.txt", dir);
    FILE *f = fopen(mf, "w");
    if (!f) return -1;
    fprintf(f, "step=%d\n%s\n", snap_1based, desc);
    fclose(f);
    snprintf(out_path, out_cap, "%s", dir);
    return 0;
}

int cos_long_horizon_run(const cos_lh_step_spec_t *specs, int n_spec,
                         float tau_accept, float tau_rethink, float tau_abort,
                         const char *snapshot_root,
                         cos_lh_report_t *rep) {
    if (!specs || !rep || !snapshot_root || snapshot_root[0] == '\0')
        return -1;
    if (n_spec < 1 || n_spec > COS_LH_MAX_STEPS) return -1;

    memset(rep, 0, sizeof *rep);
    rep->n_spec = n_spec;
    if (ensure_dir(snapshot_root) != 0) return -1;

    float sigma_first = specs[0].sigma;
    float sigma_last  = specs[0].sigma;
    float sigma_max   = specs[0].sigma;
    int   n_visit     = 0;

    for (int i = 0; i < n_spec; i++) {
        n_visit++;
        float                sig = specs[i].sigma;
        cos_sigma_action_t   a   = cos_sigma_reinforce(sig, tau_accept,
                                                       tau_rethink);
        if (sig > tau_abort) a = COS_SIGMA_ACTION_ABSTAIN;

        cos_lh_step_result_t *sr = &rep->step[i];
        sr->step_id = i + 1;
        snprintf(sr->description, sizeof sr->description, "%s",
                 specs[i].description);
        sr->sigma          = sig;
        sr->action         = a;
        sr->rethink_count  = 0;
        sr->snapshot_path[0] = '\0';

        if (sig > sigma_max) sigma_max = sig;
        sigma_last = sig;

        /* RETHINK: v0 aborts the plan (no multi-round rethink loop). */
        if (a == COS_SIGMA_ACTION_ABSTAIN || a == COS_SIGMA_ACTION_RETHINK) {
            rep->aborted            = 1;
            rep->n_executed         = i;
            rep->rollback_snapshot  = i;
            break;
        }

        if (write_snap(snapshot_root, i + 1, specs[i].description,
                       sr->snapshot_path, sizeof sr->snapshot_path) != 0) {
            rep->aborted           = 1;
            rep->n_executed        = i;
            rep->rollback_snapshot = i > 0 ? i : 0;
            break;
        }
        rep->n_executed = i + 1;
    }

    if (!rep->aborted) {
        rep->rollback_snapshot = n_spec;
        rep->n_executed      = n_spec;
    }

    rep->sigma_plan_max = sigma_max;
    if (n_visit <= 1)
        rep->dsigma_dt = 0.f;
    else
        rep->dsigma_dt = (sigma_last - sigma_first) / (float)(n_visit - 1);

    return rep->aborted ? 1 : 0;
}

static int dl_organize(const char *m) {
    if (!m) return 0;
    char low[256];
    size_t j = 0;
    for (size_t i = 0; m[i] && j + 1 < sizeof low; i++)
        low[j++] = (char)tolower((unsigned char)m[i]);
    low[j] = '\0';
    int has_dl = strstr(low, "download") != NULL;
    int has_org = strstr(low, "organize") != NULL
               || strstr(low, "organise") != NULL;
    int has_fold = strstr(low, "folder") != NULL;
    return has_dl && (has_org || has_fold);
}

int cos_long_horizon_mission_downloads(cos_lh_step_spec_t *out, int *n_out,
                                     int fail_at_step3) {
    if (!out || !n_out) return -1;
    *n_out = 4;
    snprintf(out[0].description, sizeof out[0].description,
             "Scan directory");
    out[0].sigma = 0.05f;
    snprintf(out[1].description, sizeof out[1].description,
             "Classify files");
    out[1].sigma = 0.12f;
    snprintf(out[2].description, sizeof out[2].description,
             "Create subfolders");
    out[2].sigma = fail_at_step3 ? 0.75f : 0.08f;
    snprintf(out[3].description, sizeof out[3].description,
             "Move files");
    out[3].sigma = 0.22f;
    return 0;
}

int cos_long_horizon_mission_from_text(const char *mission,
                                       cos_lh_step_spec_t *out, int *n_out,
                                       int fail_step3) {
    if (!mission || !out || !n_out) return -1;
    if (dl_organize(mission))
        return cos_long_horizon_mission_downloads(out, n_out, fail_step3);
    return -1;
}


int cos_long_horizon_self_test(void) {
    char root[256];
    snprintf(root, sizeof root, "/tmp/cos_lh_test_%d", (int)getpid());
    if (ensure_dir(root) != 0) return 1;

    cos_lh_step_spec_t sp[COS_LH_MAX_STEPS];
    int n = 0;
    if (cos_long_horizon_mission_downloads(sp, &n, 0) != 0) return 2;

    cos_lh_report_t rep;
    int rc = cos_long_horizon_run(sp, n, 0.40f, 0.60f, COS_LH_DEFAULT_TAU_ABORT,
                                  root, &rep);
    if (rc != 0) return 3;
    if (rep.aborted || rep.n_executed != 4) return 4;
    if (rep.sigma_plan_max < 0.20f || rep.sigma_plan_max > 0.25f) return 5;

    /* rollback path */
    if (cos_long_horizon_mission_downloads(sp, &n, 1) != 0) return 6;
    char root2[256];
    snprintf(root2, sizeof root2, "/tmp/cos_lh_test_rb_%d", (int)getpid());
    rc = cos_long_horizon_run(sp, n, 0.40f, 0.60f, COS_LH_DEFAULT_TAU_ABORT,
                              root2, &rep);
    if (rc != 1 || !rep.aborted) return 7;
    if (rep.rollback_snapshot != 2) return 8;
    if (rep.n_executed != 2) return 9;

    /* cleanup */
    char cmd[512];
    snprintf(cmd, sizeof cmd, "rm -rf \"%s\" \"%s\"", root, root2);
    (void)system(cmd);
    return 0;
}
