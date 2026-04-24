/*
 * cos web — σ-ranked search + optional fetch/verify (libcurl).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CLI_COS_WEB_H
#define COS_CLI_COS_WEB_H

#include "cos_search.h"

#ifdef __cplusplus
extern "C" {
#endif

int cos_web_search(const char *query, struct cos_search_result *results,
                   int max, int *n);

int cos_web_fetch_page(const char *url, char *text, int max_len);

int cos_web_verify_claim(const char *claim, int *verified);

int cos_web_main(int argc, char **argv);

int cos_web_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_CLI_COS_WEB_H */
