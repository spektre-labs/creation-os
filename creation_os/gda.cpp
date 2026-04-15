#include "gda.hpp"

#include <cmath>
#include <cctype>

namespace spektre_gda {

double gda_hash_word(const char * utf8, size_t byte_len) {
    double h = 0.0;
    for (size_t i = 0; i < byte_len; i++) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<unsigned char>(c - 'A' + 'a');
        }
        if (c >= 'a' && c <= 'z') {
            const double v = static_cast<double>(c - 'a' + 1);
            h                = (h * BASE + v) / PHI;
        }
    }
    return h;
}

double gda_interference_d(double Ha, double Hb) {
    if (Hb == 0.0) {
        return 0.0;
    }
    return (Ha * PHI) / Hb;
}

static bool is_ascii_ws(unsigned char b) {
    return b == ' ' || b == '\t' || b == '\n' || b == '\r' || b == '\f' || b == '\v';
}

std::vector<std::string> gda_split_words_utf8(const char * utf8, size_t byte_len) {
    std::vector<std::string> words;
    size_t i = 0;
    while (i < byte_len) {
        while (i < byte_len && is_ascii_ws(static_cast<unsigned char>(utf8[i]))) {
            i++;
        }
        if (i >= byte_len) {
            break;
        }
        const size_t start = i;
        while (i < byte_len && !is_ascii_ws(static_cast<unsigned char>(utf8[i]))) {
            i++;
        }
        words.emplace_back(utf8 + start, i - start);
    }
    return words;
}

std::vector<double> gda_word_hashes_utf8(const char * utf8, size_t byte_len) {
    const auto words = gda_split_words_utf8(utf8, byte_len);
    std::vector<double> out;
    out.reserve(words.size());
    for (const auto & w : words) {
        out.push_back(gda_hash_word(w.data(), w.size()));
    }
    return out;
}

void gda_stub_project_to_embd(double h, int32_t n_embd, float * out) {
    if (!out || n_embd <= 0) {
        return;
    }
    double sum_sq = 0.0;
    for (int32_t i = 0; i < n_embd; i++) {
        const double x =
                std::sin(PHI * h * static_cast<double>(i + 1)) *
                std::cos(0.01 * h * static_cast<double>(i * i + 1));
        out[i] = static_cast<float>(x);
        sum_sq += x * x;
    }
    if (sum_sq > 1e-30) {
        const float inv = static_cast<float>(1.0 / std::sqrt(sum_sq));
        for (int32_t i = 0; i < n_embd; i++) {
            out[i] *= inv;
        }
    }
}

} // namespace spektre_gda
