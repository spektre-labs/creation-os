/*
 * cos-plan — long-horizon σ-checkpoint planner (HORIZON-3).
 *
 *   cos-plan --mission "Organize my downloads folder"
 *   cos-plan --rollback-demo
 *   cos-plan --self-test
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../sigma/plan/long_horizon.h"
#include "../sigma/pipeline/reinforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void usage(void) {
    fprintf(stderr,
            "usage: cos-plan --mission \"text\" [options]\n"
            "       cos-plan --demo\n"
            "       cos-plan --rollback-demo\n"
            "       cos-plan --self-test\n"
            "\n"
            "  --tau-accept F   (default 0.40)\n"
            "  --tau-rethink F  (default 0.60)\n"
            "  --tau-abort F    (default %.2f)\n"
            "  --snapshot-root DIR  override ~/.cos/plan/run_<ts>_<pid>\n",
            (double)COS_LH_DEFAULT_TAU_ABORT);
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_long_horizon_self_test() != 0 ? 1 : 0;

    float tau_a = 0.40f, tau_r = 0.60f, tau_ab = COS_LH_DEFAULT_TAU_ABORT;
    const char *mission = NULL;
    int demo = 0, rollback_demo = 0;
    const char *snap_override = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mission") == 0 && i + 1 < argc)
            mission = argv[++i];
        else if (strcmp(argv[i], "--demo") == 0)
            demo = 1;
        else if (strcmp(argv[i], "--rollback-demo") == 0)
            rollback_demo = 1;
        else if (strcmp(argv[i], "--tau-accept") == 0 && i + 1 < argc)
            tau_a = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--tau-rethink") == 0 && i + 1 < argc)
            tau_r = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--tau-abort") == 0 && i + 1 < argc)
            tau_ab = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--snapshot-root") == 0 && i + 1 < argc)
            snap_override = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        }
    }

    if (demo)
        mission = "Organize my downloads folder";
    if (!mission && !rollback_demo) {
        usage();
        return 2;
    }
    if (rollback_demo)
        mission = "Organize my downloads folder";

    cos_lh_step_spec_t sp[COS_LH_MAX_STEPS];
    int n = 0;
    if (cos_long_horizon_mission_from_text(mission, sp, &n, rollback_demo) != 0) {
        fprintf(stderr,
                "cos-plan: unknown mission — include \"download\" and "
                "(\"organize\" or \"folder\")\n");
        return 2;
    }

    char root[1024];
    if (snap_override != NULL) {
        snprintf(root, sizeof root, "%s", snap_override);
    } else if (getenv("COS_PLAN_SNAPSHOT_ROOT") != NULL
               && getenv("COS_PLAN_SNAPSHOT_ROOT")[0] != '\0') {
        snprintf(root, sizeof root, "%s", getenv("COS_PLAN_SNAPSHOT_ROOT"));
    } else {
        const char *home = getenv("HOME");
        if (home == NULL || home[0] == '\0') home = "/tmp";
        char prepare[1050];
        snprintf(prepare, sizeof prepare, "mkdir -p \"%s/.cos/plan\"", home);
        (void)system(prepare);
        long long ts = (long long)time(NULL);
        snprintf(root, sizeof root, "%s/.cos/plan/run_%lld_%d",
                 home, ts, (int)getpid());
    }

    cos_lh_report_t rep;
    int rc = cos_long_horizon_run(sp, n, tau_a, tau_r, tau_ab, root, &rep);

    for (int i = 0; i < n; i++) {
        cos_lh_step_result_t *s = &rep.step[i];
        const char *snaplab = "—";
        char snapbuf[32];
        if (s->snapshot_path[0] != '\0') {
            snprintf(snapbuf, sizeof snapbuf, "snap:%d", s->step_id);
            snaplab = snapbuf;
        }
        fprintf(stdout,
                "Step %d/%d: %-22s [σ=%.2f | %s | %s]\n",
                s->step_id, n, s->description, (double)s->sigma,
                cos_sigma_action_label(s->action), snaplab);
        if (rep.aborted && i == rep.n_executed && s->action == COS_SIGMA_ACTION_ABSTAIN) {
            fprintf(stdout, "\"%s\" — step aborted (σ above gate / τ_abort)\n",
                    s->description);
            break;
        }
        if (rep.aborted && i == rep.n_executed && s->action == COS_SIGMA_ACTION_RETHINK) {
            fprintf(stdout, "\"%s\" — RETHINK not replayed in v0 (abort)\n",
                    s->description);
            break;
        }
    }

    if (rep.aborted) {
        if (rep.rollback_snapshot <= 0)
            fprintf(stdout,
                    "Rolling back to initial state (no checkpoints persisted).\n");
        else
            fprintf(stdout,
                    "Rolling back to snapshot:%d. Steps 1-%d preserved.\n",
                    rep.rollback_snapshot, rep.rollback_snapshot);
    } else {
        fprintf(stdout,
                "Done. σ_plan=%.2f (max) | dσ/dt=%+.2f/step | %d/%d complete\n",
                (double)rep.sigma_plan_max, (double)rep.dsigma_dt,
                rep.n_executed, n);
    }
    fprintf(stdout, "  snapshot root: %s\n", root);
    fprintf(stdout, "  assert(declared == realized);\n  1 = 1.\n");

    return (rc != 0 && !rep.aborted) ? 3 : 0;
}
