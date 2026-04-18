/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v114 σ-Swarm — multi-specialist routing with σ-product-min
 * arbitration.  This is the Proconductor principle shipped as a
 * product: several small, specialist GGUF models each answer a
 * task; the one with the lowest σ-product wins, and if two models
 * independently converge on the same answer at low σ, we emit a
 * "resonance" (not "consensus" — we're measurement-first) flag.
 *
 * Current v114.0 scope (deliberately narrow so the merge-gate stays
 * green without downloading gigabytes of GGUF weights):
 *
 *   - Config layer: TOML-ish `[swarm]` section → list of up to four
 *     `cos_v114_specialist_t` entries (name, gguf path, role tag).
 *   - Arbitration layer: given `n` candidate responses (plain text
 *     + σ profile + σ_product), pick the winner by σ_product_min,
 *     compute the runner-up, decide resonance.
 *   - Response shaping: write the X-COS-Specialist / -Sigma /
 *     -Runner-Up / -Consensus HTTP headers; JSON choices payload.
 *   - Stub runner: for CI and "no GGUF yet" scenarios, produce
 *     deterministic synthetic candidates from the prompt hash so
 *     the arbitration layer can be exercised end-to-end.
 *
 * Out of scope for v114.0 (documented, not implemented):
 *   - True parallel inference over N GGUFs (requires threading the
 *     llama_context, which is not thread-safe; see
 *     docs/v114/README.md for the deferred design).
 *   - Dynamic role inference (tasks → specialist) beyond the fixed
 *     "ask everyone" policy.  v115 will add a σ-triaging layer.
 *
 * None of this is hidden: the swarm's self-test prints
 *   "v114 σ-Swarm: arbitration=sigma_product_min,
 *    parallel=sequential, specialists=<N>"
 * and the HTTP response carries the `parallel_mode` field so
 * consumers know what they actually got.
 */
#ifndef COS_V114_SWARM_H
#define COS_V114_SWARM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V114_MAX_SPECIALISTS   8
#define COS_V114_NAME_MAX          64
#define COS_V114_ROLE_MAX          128
#define COS_V114_PATH_MAX          1024
#define COS_V114_CONTENT_MAX       4096

typedef struct cos_v114_specialist {
    char   name[COS_V114_NAME_MAX];
    char   role[COS_V114_ROLE_MAX];
    char   gguf_path[COS_V114_PATH_MAX];
    int    loaded;                /* 1 iff cos_v101_bridge_open
                                   * succeeded at server startup */
} cos_v114_specialist_t;

typedef struct cos_v114_config {
    cos_v114_specialist_t specialists[COS_V114_MAX_SPECIALISTS];
    size_t                n;
    /* Arbitration knobs. */
    float                 consensus_epsilon;    /* default 0.15 —
                                                   runner-up σ must
                                                   be within epsilon
                                                   of winner to count
                                                   as resonance */
    int                   consensus_threshold;  /* default 2 — how
                                                   many specialists
                                                   must agree */
    char                  routing[32];          /* default
                                                   "sigma_product_min" */
} cos_v114_config_t;

typedef struct cos_v114_candidate {
    const char *specialist_name;       /* borrowed; NUL-terminated */
    const char *role;                  /* borrowed */
    char        content[COS_V114_CONTENT_MAX];
    float       sigma_profile[8];
    float       sigma_mean;
    float       sigma_product;
    int         abstained;             /* from the specialist's own
                                          σ-gate */
} cos_v114_candidate_t;

typedef struct cos_v114_decision {
    int    winner_index;               /* -1 when all abstained */
    int    runner_up_index;            /* -1 when <2 candidates */
    float  winner_sigma;
    float  runner_up_sigma;
    int    resonance;                  /* 1 iff >= threshold
                                          specialists agree */
    int    all_abstained;
    char   routing[32];                /* echoes config */
    char   parallel_mode[32];          /* "sequential"|"parallel" */
} cos_v114_decision_t;

/* Defaults. */
void cos_v114_config_defaults(cos_v114_config_t *cfg);

/* Parse a TOML-ish [swarm] block out of a config file text blob.
 * Accepts:
 *   [swarm]
 *   specialists = [
 *     { name = "reasoning", gguf = "bitnet-2b.gguf", role = "math" },
 *     { name = "code",      gguf = "qwen-7b.gguf",   role = "programming" }
 *   ]
 *   routing = "sigma_product_min"
 *   consensus_threshold = 2
 *   consensus_epsilon   = 0.15
 * Returns number of specialists parsed, -1 on malformed input. */
int cos_v114_parse_config(const char *toml_text, cos_v114_config_t *out);

/* Arbitrate.  `cands` is an array of `n` candidates (0 <= n <=
 * COS_V114_MAX_SPECIALISTS).  Returns 0 on success, -1 on bad
 * inputs.  The decision struct carries the verdict. */
int cos_v114_arbitrate(const cos_v114_candidate_t *cands, size_t n,
                       const cos_v114_config_t *cfg,
                       cos_v114_decision_t *out);

/* Deterministic stub runner — given a prompt and a configured swarm,
 * synthesize per-specialist candidates by hashing (prompt, role).
 * σ-product is biased by role match: a "code"-roled specialist
 * answering a "what is 2+2" prompt gets a lower σ than a "general"-
 * roled one, etc.  This is *not* a real model: it's a shape check
 * for the arbitration path used by CI and by the /v1/chat/completions
 * endpoint when the specialists are not loaded yet. */
int cos_v114_stub_run(const cos_v114_config_t *cfg,
                      const char *prompt,
                      cos_v114_candidate_t *out,
                      size_t cap);

/* Write the choices[] payload and the X-COS-* header lines.  The
 * caller owns the HTTP framing; this just fills two buffers:
 *   headers_out: "X-COS-Specialist: ...\r\nX-COS-Specialist-Sigma: ...\r\n..."
 *   body_out:    OpenAI chat.completion shape with a swarm block.
 * Returns 0 on success, -1 on overflow. */
int cos_v114_build_response(const cos_v114_candidate_t *cands, size_t n,
                            const cos_v114_decision_t *dec,
                            const char *model_id, long created,
                            char *headers_out, size_t hcap,
                            char *body_out, size_t bcap);

/* Pure-C smoke: parser + arbitrator + stub runner.  Returns 0 iff
 * all-pass. */
int cos_v114_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V114_SWARM_H */
