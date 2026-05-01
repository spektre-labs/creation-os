/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-5 — meta-cognitive σ across awareness channels. */

#include "introspection.h"

#include <stdio.h>
#include <string.h>

float cos_ultra_metacog_max_sigma(const cos_ultra_metacog_levels_t *lv) {
    if (!lv) return 1.0f;
    float m = lv->sigma_perception;
    if (lv->sigma_self > m) m = lv->sigma_self;
    if (lv->sigma_social > m) m = lv->sigma_social;
    if (lv->sigma_situational > m) m = lv->sigma_situational;
    if (m != m) return 1.0f;
    return m;
}

void cos_ultra_metacog_recommend(const cos_ultra_metacog_levels_t *lv,
                                 char *buf, size_t cap) {
    if (!buf || cap == 0) return;
    buf[0] = '\0';
    if (!lv) return;
    float sp = lv->sigma_perception, ss = lv->sigma_self;
    float so = lv->sigma_social, st = lv->sigma_situational;
    if (so >= sp && so >= ss && so >= st && so > 0.35f) {
        snprintf(buf, cap, "%s", "Ask a clarifying question (social channel uncertain).");
        return;
    }
    if (sp > 0.45f) {
        snprintf(buf, cap, "%s", "Re-read the prompt; perception channel is noisy.");
        return;
    }
    snprintf(buf, cap, "%s", "Proceed with generation; all channels within band.");
}

void cos_ultra_metacog_emit_report(FILE *fp,
                                   const cos_ultra_metacog_levels_t *lv) {
    if (!fp || !lv) return;
    fprintf(fp, "ULTRA-5 meta-cognition (σ per awareness channel)\n");
    fprintf(fp, "  Perception:   σ=%.2f\n", (double)lv->sigma_perception);
    fprintf(fp, "  Self:         σ=%.2f\n", (double)lv->sigma_self);
    fprintf(fp, "  Social:       σ=%.2f\n", (double)lv->sigma_social);
    fprintf(fp, "  Situational:  σ=%.2f\n", (double)lv->sigma_situational);
    fprintf(fp, "  σ_meta(max):  %.2f\n", (double)cos_ultra_metacog_max_sigma(lv));
    char r[160];
    cos_ultra_metacog_recommend(lv, r, sizeof r);
    fprintf(fp, "  Action:       %s\n", r);
}

int cos_ultra_metacog_self_test(void) {
    cos_ultra_metacog_levels_t L = {
        .sigma_perception = 0.08f,
        .sigma_self = 0.15f,
        .sigma_social = 0.42f,
        .sigma_situational = 0.12f,
    };
    if (cos_ultra_metacog_max_sigma(&L) < 0.41f) return 1;
    char b[200];
    cos_ultra_metacog_recommend(&L, b, sizeof b);
    if (strstr(b, "clarifying") == NULL) return 2;
    return 0;
}
