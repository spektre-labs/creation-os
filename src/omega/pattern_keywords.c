/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "pattern_keywords.h"

#include <stdio.h>
#include <string.h>

void cos_pattern_keyword_domain(const char *prompt, char *dom, size_t dcap)
{
    const char *p = prompt ? prompt : "";
    if (dom == NULL || dcap < 2) {
        if (dom && dcap >= 1)
            dom[0] = '\0';
        return;
    }
    if (strstr(p, "capital of") != NULL || strstr(p, "Where is") != NULL
        || strstr(p, "where is") != NULL) {
        snprintf(dom, dcap, "%s", "geography");
        return;
    }
    if (strstr(p, "how many") != NULL || strstr(p, "What is") != NULL
        || strstr(p, "calculate") != NULL || strstr(p, "What comes next")
        != NULL) {
        snprintf(dom, dcap, "%s", "math_factual");
        return;
    }
    if (strstr(p, "write a") != NULL || strstr(p, "describe") != NULL) {
        snprintf(dom, dcap, "%s", "creative");
        return;
    }
    if (strstr(p, "what do you") != NULL || strstr(p, "are you") != NULL) {
        snprintf(dom, dcap, "%s", "self_aware");
        return;
    }
    if (strstr(p, "predict") != NULL || strstr(p, "what will") != NULL) {
        snprintf(dom, dcap, "%s", "speculative");
        return;
    }
    snprintf(dom, dcap, "%s", "general");
}
