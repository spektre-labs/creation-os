/* Neural–symbolic bridge — constitution + τ gate digest. */

#include "neural_symbolic.h"

#include "../codex/codex_smt.h"
#include "../sigma/constitution.h"
#include "../license_kernel/license_attest.h"

#include <stdio.h>
#include <string.h>

int cos_bridge_evaluate(const char *prompt, const char *response, float sigma,
                        float tau_accept, float tau_rethink,
                        cos_bridge_result_t *out)
{
    char        pack[4096];
    int         v = 0, mv = 0;

    if (out == NULL)
        return -1;
    memset(out, 0, sizeof *out);
    out->sigma_combined = sigma;
    out->tau_accept     = tau_accept;
    out->tau_rethink    = tau_rethink;

    (void)cos_codex_smt_evaluate(response, sigma, out->violation_summary,
                                 sizeof out->violation_summary, &v, &mv);

    out->constitution_violations         = v;
    out->constitution_mandatory_hits     = mv;
    out->gate = cos_sigma_reinforce(sigma, tau_accept, tau_rethink);
    if (mv > 0)
        out->gate = COS_SIGMA_ACTION_ABSTAIN;

    snprintf(pack, sizeof pack, "%s|%s|%.6g|%d",
             prompt != NULL ? prompt : "",
             response != NULL ? response : "", (double)sigma, (int)out->gate);
    spektre_sha256((const uint8_t *)pack, strlen(pack), out->bridge_digest);
    return 0;
}

int cos_neural_symbolic_self_test(void)
{
    cos_bridge_result_t br;

    memset(&br, 0, sizeof br);
    if (cos_bridge_evaluate("p", "ok", 0.25f, 0.40f, 0.60f, &br) != 0)
        return 1;
    if (br.gate != COS_SIGMA_ACTION_ACCEPT)
        return 2;
    memset(&br, 0, sizeof br);
    if (cos_bridge_evaluate("p", "ok", 0.95f, 0.40f, 0.60f, &br) != 0)
        return 3;
    if (br.gate != COS_SIGMA_ACTION_ABSTAIN)
        return 4;
    return 0;
}
