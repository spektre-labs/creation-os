/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v42 lab helpers: deterministic “quick σ” from short text for scaffolding (not an LM).
 */
#ifndef CREATION_OS_V42_TEXT_H
#define CREATION_OS_V42_TEXT_H

#include "../sigma/decompose.h"

/** Fill n logits from a stable hash of text (Dirichlet decomposition input). */
void v42_scratch_logits_from_text(const char *t, float *log, int n);

/** Epistemic channel from Dirichlet map over scratch logits. */
float v42_epistemic_from_text(const char *t);

#endif /* CREATION_OS_V42_TEXT_H */
