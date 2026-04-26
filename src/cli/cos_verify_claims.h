/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * cos verify — optional stdin / --file path: per-sentence semantic-entropy σ.
 */
#ifndef COS_VERIFY_CLAIMS_H
#define COS_VERIFY_CLAIMS_H

#include <stddef.h>

int cos_verify_claims_main(int argc, char **argv);

/* Build {"sentences":[{text,sigma,action},...]} for HTTP /v1/verify.
 * Returns 0 on success, -1 on overflow/encode error, -2 if text too long. */
int cos_verify_claims_sentences_json(const char *text, char *out, size_t outcap);

#endif
