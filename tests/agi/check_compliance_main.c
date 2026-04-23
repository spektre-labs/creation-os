#include "compliance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PA(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s\n", msg); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    PA(cos_compliance_self_test() == 0, "self_test");

    {
        struct cos_proof_receipt r;
        struct cos_compliance_check c;

        memset(&r, 0, sizeof r);
        r.receipt_hash[0] = 1;
        r.sigma_combined    = 0.5f;
        r.within_compute_budget = 1;
        PA(cos_compliance_check_output(&r, &c) == 0, "check_basic");
        PA(c.risk_classification >= 1 && c.risk_classification <= 4,
           "risk_bounds");
    }

    {
        struct cos_compliance_check c;
        char *rep;

        memset(&c, 0, sizeof c);
        c.eu_ai_act_compliant = 1;
        snprintf(c.violations, sizeof c.violations, "%s", "");
        rep = cos_compliance_report(&c);
        PA(rep != NULL && strstr(rep, "eu_ai_act_compliant") != NULL,
           "report_json");
        free(rep);
    }

    {
        struct cos_proof_receipt r;
        struct cos_compliance_check c;

        memset(&r, 0, sizeof r);
        r.receipt_hash[0] = 1;
        r.sigma_combined = 0.95f;
        r.within_compute_budget = 0;
        PA(cos_compliance_check_output(&r, &c) == 0, "budget_violation");
        PA(strstr(c.violations, "budget") != NULL || c.eu_ai_act_compliant == 0,
           "budget_flag");
    }

    return 0;
}
