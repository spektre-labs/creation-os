#pragma once

// Phase 6.0 — Geometric Deterministic Addressing (GDA)
//
// Core hash and interference are deterministic C++ (no BPE).
//
// Physical constraint: pretrained Llama weights assume the training tokenizer’s
// discrete ID → W_te mapping. Replacing that with GDA scalars requires a learned
// projection W_gda ∈ R^{n_embd×1} (or higher-rank) trained with the model.
// gda_stub_project_to_embd() is a deterministic placeholder so callers can drive
// llama_batch.embd (token == NULL) until W_gda is available.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spektre_gda {

constexpr double PHI = 1.61803398874989484820; // φ (more precise than 1.6180339887)
constexpr double BASE = 27.0;

/// Division-chain hash over lowercase a..z only (other code units skipped).
/// Per step: h ← (h · BASE + v) / φ, with v = ord(c) − ord('a') + 1 ∈ [1, 26].
double gda_hash_word(const char * utf8, size_t byte_len);

inline double gda_hash_word(const std::string & w) {
    return gda_hash_word(w.data(), w.size());
}

/// Relational geometry: (H_a · φ) / H_b. If H_b == 0, returns 0.
double gda_interference_d(double Ha, double Hb);

inline double gda_interference_words(const char * a, size_t la, const char * b, size_t lb) {
    return gda_interference_d(gda_hash_word(a, la), gda_hash_word(b, lb));
}

/// Split UTF-8 text on ASCII whitespace; words are maximal non-whitespace runs.
std::vector<std::string> gda_split_words_utf8(const char * utf8, size_t byte_len);

/// One scalar hash per word (same order as gda_split_words_utf8).
std::vector<double> gda_word_hashes_utf8(const char * utf8, size_t byte_len);

/// Deterministic R^1 → R^{n_embd} map (L2-normalized). Not a trained W_gda.
void gda_stub_project_to_embd(double h, int32_t n_embd, float * out);

} // namespace spektre_gda
