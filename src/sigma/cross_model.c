/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "cross_model.h"

#include "bitnet_server.h"
#include "reinforce.h"
#include "semantic_entropy.h"

#include <stdlib.h>
#include <string.h>

int cos_cross_model_query(int port, const char *prompt, const char **models,
                          int n_models, cos_model_result_t *results)
{
    int i;
    if (prompt == NULL || models == NULL || results == NULL || n_models <= 0)
        return -1;
    for (i = 0; i < n_models; ++i) {
        cos_model_result_t *r = &results[i];
        memset(r, 0, sizeof *r);
        snprintf(r->model, sizeof r->model, "%s",
                 (models[i] != NULL && models[i][0] != '\0') ? models[i]
                                                            : "gemma3:4b");
        char *resp =
            cos_bitnet_query_model(port, prompt, NULL, r->model, 0.3f, 256);
        if (resp != NULL) {
            snprintf(r->response, sizeof r->response, "%s", resp);
            free(resp);
        }
        int   ncl = 3;
        float se  = cos_semantic_entropy_ex(
            r->response[0] != '\0' ? r->response : prompt, NULL, port,
            r->model, 3, &ncl);
        r->sigma = se;
        {
            cos_sigma_action_t a = cos_sigma_reinforce(se, 0.40f, 0.60f);
            snprintf(r->action, sizeof r->action, "%s",
                     cos_sigma_action_label(a));
        }
    }
    return 0;
}

int cos_select_best_model(const cos_model_result_t *results, int n_models)
{
    int   best = 0;
    float lo   = 1e9f;
    int   i;
    if (results == NULL || n_models <= 0)
        return 0;
    for (i = 0; i < n_models; ++i) {
        if (results[i].sigma < lo) {
            lo   = results[i].sigma;
            best = i;
        }
    }
    return best;
}
