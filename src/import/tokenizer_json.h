/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Minimal HuggingFace-style tokenizer.json probe: count entries under "model"."vocab"
 * when values are JSON numbers (integers). Returns -1 on I/O or parse failure.
 */
#ifndef TOKENIZER_JSON_H
#define TOKENIZER_JSON_H

#include <stdint.h>

int cos_tokenizer_json_count_vocab(const char *path, uint64_t *out_n);

#endif /* TOKENIZER_JSON_H */
