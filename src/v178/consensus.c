/*
 * v178 σ-Consensus — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "consensus.h"
#include "license_attest.h"          /* spektre_sha256 */

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------- */
/* Fixture                                                       */
/* ------------------------------------------------------------- */

/* Per-claim σ_i table.  Rows = claims; cols = nodes N0..N4.
 *
 * N0, N1, N2: honest, mature (rep = 3.0)
 * N3: honest, young       (rep = 1.0)
 * N4: byzantine           (rep = 0.5, suspect; always flips)
 *
 * With θ = 0.30 (true iff σ < θ):
 *
 *   C0  honest say TRUE  (σ ≈ 0.10), byz says FALSE (σ = 0.99)  → ACCEPT
 *   C1  honest say FALSE (σ ≈ 0.80), byz says TRUE  (σ = 0.05)  → REJECT
 *   C2  honest say TRUE  with one young unsure (σ = 0.35);
 *        byz says FALSE                                        → ACCEPT
 *   C3  honest SPLIT 2-TRUE / 2-FALSE, byz FALSE                → ABSTAIN
 */
static const float FIXTURE_SIGMA[COS_V178_N_CLAIMS][COS_V178_N_NODES] = {
    /* C0 ACCEPT */   { 0.10f, 0.12f, 0.08f, 0.15f, 0.99f },
    /* C1 REJECT */   { 0.80f, 0.78f, 0.82f, 0.75f, 0.05f },
    /* C2 ACCEPT */   { 0.09f, 0.11f, 0.13f, 0.35f, 0.95f },
    /* C3 ABSTAIN */  { 0.10f, 0.45f, 0.12f, 0.55f, 0.90f },
};
static const char *CLAIM_LABELS[COS_V178_N_CLAIMS] = {
    "is the mesh online",
    "host has free gpu slot",
    "reviewer bot is trustworthy",
    "emit snapshot to origin",
};

void cos_v178_init(cos_v178_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x178BEEFE0178ULL;
    s->theta_sigma  = 0.30f;
    s->quorum       = 2.0f / 3.0f;
    s->rep_newbie   = 1.0f;
    s->rep_mature   = 3.0f;
    s->rep_suspect  = 0.5f;

    static const char *NAMES[COS_V178_N_NODES] = {
        "N0-alpha", "N1-beta", "N2-gamma", "N3-young", "N4-byz",
    };
    static const bool  BYZ[COS_V178_N_NODES]   = {
        false, false, false, false, true,
    };
    static const float REPS[COS_V178_N_NODES]  = {
        3.0f, 3.0f, 3.0f, 1.0f, 0.5f,
    };
    for (int i = 0; i < COS_V178_N_NODES; ++i) {
        s->nodes[i].id         = i;
        size_t n = strlen(NAMES[i]);
        if (n >= sizeof(s->nodes[i].name)) n = sizeof(s->nodes[i].name) - 1;
        memcpy(s->nodes[i].name, NAMES[i], n);
        s->nodes[i].name[n]    = '\0';
        s->nodes[i].byzantine  = BYZ[i];
        s->nodes[i].reputation = REPS[i];
    }
}

/* ------------------------------------------------------------- */
/* Agreement                                                     */
/* ------------------------------------------------------------- */

static void make_leaf(const cos_v178_claim_t *c, uint8_t out[COS_V178_HASH_LEN]) {
    /* deterministic leaf = SHA-256 of packed σ + decision */
    uint8_t buf[COS_V178_N_NODES * 4 + 8];
    size_t  off = 0;
    /* claim id + decision */
    uint32_t id  = (uint32_t)c->claim_id;
    uint32_t dec = (uint32_t)c->decision;
    memcpy(buf + off, &id,  4); off += 4;
    memcpy(buf + off, &dec, 4); off += 4;
    for (int i = 0; i < COS_V178_N_NODES; ++i) {
        /* σ encoded as u32 fixed-point with 1e6 scale */
        uint32_t v = (uint32_t)(c->sigma[i] * 1000000.0f);
        memcpy(buf + off, &v, 4); off += 4;
    }
    spektre_sha256(buf, off, out);
}

static void node_pair_hash(const uint8_t *a, const uint8_t *b,
                           uint8_t out[COS_V178_HASH_LEN]) {
    uint8_t joined[2 * COS_V178_HASH_LEN];
    memcpy(joined,                       a, COS_V178_HASH_LEN);
    memcpy(joined + COS_V178_HASH_LEN,   b, COS_V178_HASH_LEN);
    spektre_sha256(joined, sizeof(joined), out);
}

static void merkle_root(const cos_v178_state_t *s,
                        uint8_t out[COS_V178_HASH_LEN]) {
    /* Classic merkle: duplicate last leaf if odd. */
    uint8_t level[COS_V178_N_CLAIMS + 1][COS_V178_HASH_LEN];
    int n = s->n_claims;
    if (n <= 0) { memset(out, 0, COS_V178_HASH_LEN); return; }
    for (int i = 0; i < n; ++i)
        memcpy(level[i], s->claims[i].leaf_hash, COS_V178_HASH_LEN);
    while (n > 1) {
        int new_n = 0;
        for (int i = 0; i < n; i += 2) {
            const uint8_t *L = level[i];
            const uint8_t *R = (i + 1 < n) ? level[i + 1] : level[i];
            uint8_t tmp[COS_V178_HASH_LEN];
            node_pair_hash(L, R, tmp);
            memcpy(level[new_n++], tmp, COS_V178_HASH_LEN);
        }
        n = new_n;
    }
    memcpy(out, level[0], COS_V178_HASH_LEN);
}

void cos_v178_run(cos_v178_state_t *s) {
    s->n_claims = COS_V178_N_CLAIMS;
    int accept = 0, reject = 0, abstain = 0;
    for (int c = 0; c < COS_V178_N_CLAIMS; ++c) {
        cos_v178_claim_t *cl = &s->claims[c];
        memset(cl, 0, sizeof(*cl));
        cl->claim_id = c;
        size_t n = strlen(CLAIM_LABELS[c]);
        if (n >= sizeof(cl->label)) n = sizeof(cl->label) - 1;
        memcpy(cl->label, CLAIM_LABELS[c], n);
        cl->label[n] = '\0';

        float w_true = 0.0f, w_false = 0.0f, w_total = 0.0f;
        for (int i = 0; i < COS_V178_N_NODES; ++i) {
            cl->sigma[i] = FIXTURE_SIGMA[c][i];
            bool vote = cl->sigma[i] < s->theta_sigma;
            cl->vote_true[i] = vote;
            float w = s->nodes[i].reputation;
            w_total += w;
            if (vote) w_true  += w;
            else      w_false += w;
        }
        cl->weight_true  = w_true;
        cl->weight_false = w_false;
        if (w_total > 0.0f && (w_true / w_total) >= s->quorum)
            cl->decision = COS_V178_DEC_ACCEPT;
        else if (w_total > 0.0f && (w_false / w_total) >= s->quorum)
            cl->decision = COS_V178_DEC_REJECT;
        else
            cl->decision = COS_V178_DEC_ABSTAIN;

        /* update reputations — sybil-resistance: new nodes with
         * weight 1.0 contribute but cannot override mature nodes. */
        if (cl->decision != COS_V178_DEC_ABSTAIN) {
            bool truth = (cl->decision == COS_V178_DEC_ACCEPT);
            for (int i = 0; i < COS_V178_N_NODES; ++i) {
                if (cl->vote_true[i] == truth) {
                    s->nodes[i].n_correct++;
                    s->nodes[i].reputation += 0.25f;
                } else {
                    s->nodes[i].n_incorrect++;
                    /* byzantine reputations sink faster */
                    float drop = s->nodes[i].byzantine ? 0.40f : 0.20f;
                    s->nodes[i].reputation -= drop;
                    if (s->nodes[i].reputation < 0.0f)
                        s->nodes[i].reputation = 0.0f;
                }
            }
        }

        make_leaf(cl, cl->leaf_hash);
        switch (cl->decision) {
            case COS_V178_DEC_ACCEPT:  accept++;  break;
            case COS_V178_DEC_REJECT:  reject++;  break;
            case COS_V178_DEC_ABSTAIN: abstain++; break;
        }
    }
    s->n_accept  = accept;
    s->n_reject  = reject;
    s->n_abstain = abstain;
    merkle_root(s, s->merkle_root);
}

int cos_v178_verify_merkle(const cos_v178_state_t *s) {
    uint8_t expected[COS_V178_HASH_LEN];
    merkle_root(s, expected);
    return memcmp(expected, s->merkle_root, COS_V178_HASH_LEN) == 0 ? 0 : 1;
}

void cos_v178_merkle_root_hex(const cos_v178_state_t *s,
                              char out[2 * COS_V178_HASH_LEN + 1]) {
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < COS_V178_HASH_LEN; ++i) {
        out[2 * i]     = H[(s->merkle_root[i] >> 4) & 0xF];
        out[2 * i + 1] = H[s->merkle_root[i] & 0xF];
    }
    out[2 * COS_V178_HASH_LEN] = '\0';
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

static const char *dec_name(cos_v178_decision_t d) {
    switch (d) {
        case COS_V178_DEC_ACCEPT:  return "accept";
        case COS_V178_DEC_REJECT:  return "reject";
        case COS_V178_DEC_ABSTAIN: return "abstain";
    }
    return "?";
}

size_t cos_v178_to_json(const cos_v178_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    char hex[2 * COS_V178_HASH_LEN + 1];
    cos_v178_merkle_root_hex(s, hex);
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v178\",\"theta_sigma\":%.4f,\"quorum\":%.4f,"
        "\"n_nodes\":%d,\"n_claims\":%d,\"n_accept\":%d,"
        "\"n_reject\":%d,\"n_abstain\":%d,\"merkle_root\":\"%s\","
        "\"nodes\":[",
        (double)s->theta_sigma, (double)s->quorum,
        COS_V178_N_NODES, s->n_claims,
        s->n_accept, s->n_reject, s->n_abstain, hex);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V178_N_NODES; ++i) {
        const cos_v178_node_t *nd = &s->nodes[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":%d,\"name\":\"%s\",\"byzantine\":%s,"
            "\"reputation\":%.4f,\"n_correct\":%d,\"n_incorrect\":%d}",
            i == 0 ? "" : ",", nd->id, nd->name,
            nd->byzantine ? "true" : "false",
            (double)nd->reputation, nd->n_correct, nd->n_incorrect);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"claims\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int c = 0; c < s->n_claims; ++c) {
        const cos_v178_claim_t *cl = &s->claims[c];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":%d,\"label\":\"%s\",\"weight_true\":%.4f,"
            "\"weight_false\":%.4f,\"decision\":\"%s\",\"sigma\":[",
            c == 0 ? "" : ",", cl->claim_id, cl->label,
            (double)cl->weight_true, (double)cl->weight_false,
            dec_name(cl->decision));
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
        for (int i = 0; i < COS_V178_N_NODES; ++i) {
            n = snprintf(buf + used, cap - used, "%s%.4f",
                         i == 0 ? "" : ",", (double)cl->sigma[i]);
            if (n < 0 || (size_t)n >= cap - used) return 0;
            used += (size_t)n;
        }
        n = snprintf(buf + used, cap - used, "]}");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v178_self_test(void) {
    cos_v178_state_t s;
    cos_v178_init(&s, 0x178BEEFE0178ULL);
    cos_v178_run(&s);

    /* 4 claims: 2 accept, 1 reject, 1 abstain (per fixture) */
    if (s.n_accept  != 2) return 1;
    if (s.n_reject  != 1) return 2;
    if (s.n_abstain != 1) return 3;

    /* Byzantine node always voted against majority on the non-
     * abstain claims: its reputation must be < init (0.5). */
    if (!(s.nodes[4].reputation < 0.5f)) return 4;

    /* Mature honest nodes gained reputation on resolved claims. */
    if (!(s.nodes[0].reputation > 3.0f)) return 5;
    if (!(s.nodes[1].reputation > 3.0f)) return 6;

    /* Sybil-resistance: the young node (N3) has rep ≥ start value
     * only if it voted with the majority on accepted claims.  In
     * the fixture N3 is unsure on C2 (σ=0.35 > θ so votes FALSE
     * vs ACCEPT) — so one miss.  Rep change: +0.25 (C0) +0.25 (C1)
     * −0.20 (C2) = +0.30.  Start 1.0 → 1.30. */
    if (!(s.nodes[3].reputation >= 1.29f && s.nodes[3].reputation <= 1.31f))
        return 7;

    /* Merkle root is non-zero and verifies. */
    bool any = false;
    for (int i = 0; i < COS_V178_HASH_LEN; ++i)
        if (s.merkle_root[i]) { any = true; break; }
    if (!any) return 8;
    if (cos_v178_verify_merkle(&s) != 0) return 9;

    /* Tamper the last leaf → verification must fail. */
    cos_v178_state_t t = s;
    t.claims[COS_V178_N_CLAIMS - 1].leaf_hash[0] ^= 0xFF;
    if (cos_v178_verify_merkle(&t) == 0) return 10;

    /* Determinism */
    cos_v178_state_t a, b;
    cos_v178_init(&a, 0x178BEEFE0178ULL);
    cos_v178_init(&b, 0x178BEEFE0178ULL);
    cos_v178_run(&a);
    cos_v178_run(&b);
    char A[16384], B[16384];
    size_t na = cos_v178_to_json(&a, A, sizeof(A));
    size_t nb = cos_v178_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 11;
    if (memcmp(A, B, na) != 0) return 12;

    return 0;
}
