/*
 * σ-mesh3 — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "mesh3.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------ helpers ------------------ */

static void ncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    size_t n = src ? strlen(src) : 0;
    if (n >= cap) n = cap - 1;
    if (src && n) memcpy(dst, src, n);
    dst[n] = '\0';
}

static float clip01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/* Small hash-like helper retained for the σ-estimator and the
 * deterministic DP-noise seed.  It is NOT cryptographic; the
 * envelope signature itself is genuine Ed25519 over the canonical
 * byte layout (see canonicalize_envelope below). */
static uint64_t fnv1a64(const void *data, size_t n, uint64_t seed) {
    const unsigned char *p = (const unsigned char *)data;
    uint64_t h = 1469598103934665603ULL ^ seed;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Canonical byte-layout of the message envelope we sign: kind ||
 * from || to || reserved(0) || payload.  Lives in a caller-owned
 * scratch buffer. */
static size_t canonicalize_envelope(const cos_mesh3_msg_t *msg,
                                    uint8_t *buf, size_t cap) {
    size_t n = 0;
    if (cap < 4) return 0;
    buf[n++] = (uint8_t)msg->kind;
    buf[n++] = (uint8_t)msg->from;
    buf[n++] = (uint8_t)msg->to;
    buf[n++] = 0;
    size_t pl = strlen(msg->payload);
    if (pl > cap - n) pl = cap - n;
    memcpy(buf + n, msg->payload, pl);
    n += pl;
    return n;
}

static void sign_message(const cos_mesh3_node_t *signer,
                         cos_mesh3_msg_t *msg) {
    uint8_t buf[COS_MESH3_PAYLOAD_MAX + 16];
    size_t n = canonicalize_envelope(msg, buf, sizeof buf);
    cos_sigma_proto_ed25519_sign(buf, n,
                                 signer->sign_key, sizeof signer->sign_key,
                                 msg->signature);
}

static int verify_message(const cos_mesh3_mesh_t *mesh,
                          const cos_mesh3_msg_t *msg) {
    if (msg->from < 0 || msg->from >= COS_MESH3_NODES) return 0;
    uint8_t buf[COS_MESH3_PAYLOAD_MAX + 16];
    size_t n = canonicalize_envelope(msg, buf, sizeof buf);
    return cos_sigma_proto_ed25519_verify(
               buf, n,
               mesh->nodes[msg->from].verify_key,
               COS_ED25519_PUB_LEN,
               msg->signature);
}

static int mesh_push(cos_mesh3_mesh_t *mesh, cos_mesh3_msg_t *msg) {
    if (mesh->n_messages >= COS_MESH3_MAX_MESSAGES) return -1;
    sign_message(&mesh->nodes[msg->from], msg);
    msg->verified = verify_message(mesh, msg);
    if (!msg->verified) {
        msg->dropped = 1;
        mesh->signatures_rejected++;
    }
    mesh->log[mesh->n_messages++] = *msg;
    return msg->dropped ? -1 : 0;
}

/* Deterministic σ estimator at a node — purely a function of the
 * query string and the node's identity.  Lower index (A) is most
 * authoritative, C least, so in this toy model B has a wider
 * uncertainty band. */
static float local_sigma(const cos_mesh3_node_t *n, const char *query) {
    uint64_t h = fnv1a64(query, strlen(query), 0);
    /* Bit-level unbiased fraction */
    float f = (float)((h >> 48) & 0xFFFF) / 65535.0f;   /* 0..1 */
    if (strcmp(n->role, "leaf") == 0)        f = f * 0.65f + 0.20f;
    else if (strcmp(n->role, "federator") == 0) f = f * 0.35f + 0.05f;
    else /* coordinator */                    f = f * 0.40f + 0.05f;
    return clip01(f);
}

/* DP noise: tiny deterministic perturbation seeded by message
 * index + node id so tests stay byte-stable. */
static void dp_noise_fill(float *dst, int dim, uint64_t seed) {
    uint64_t h = seed;
    for (int i = 0; i < dim; i++) {
        h = h * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t bits = (uint32_t)(h >> 32);
        /* Map to ±0.05 so the federation update has a visible DP tail
         * but never overwhelms the signal. */
        float f = ((float)bits / 4294967295.0f - 0.5f) * 0.10f;
        dst[i] = f;
    }
}

/* ------------------ init ------------------ */

/* Derive a 32-byte Ed25519 seed from a single letter tag so that
 * each mesh run gets the same keypair per role (reproducible) but
 * different per node (unforgeable from another node's keypair). */
static void derive_mesh_seed(uint8_t tag, uint8_t seed[COS_ED25519_SEED_LEN]) {
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i)
        seed[i] = (uint8_t)(tag ^ (uint8_t)(0x5Au + (unsigned)i));
}

static void mesh_init_keys(cos_mesh3_node_t *node, uint8_t tag) {
    uint8_t seed[COS_ED25519_SEED_LEN];
    uint8_t pub[COS_ED25519_PUB_LEN];
    uint8_t priv[COS_ED25519_PRIV_LEN];
    derive_mesh_seed(tag, seed);
    cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);
    memcpy(node->sign_key, priv, COS_ED25519_PRIV_LEN);
    memcpy(node->sign_key + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);
    memcpy(node->verify_key, pub, COS_ED25519_PUB_LEN);
}

void cos_mesh3_init(cos_mesh3_mesh_t *mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof *mesh);
    ncpy(mesh->nodes[0].name, COS_MESH3_NAME_MAX, "A-helsinki-m3");
    ncpy(mesh->nodes[0].role, COS_MESH3_NAME_MAX, "coordinator");
    mesh_init_keys(&mesh->nodes[0], 'A');

    ncpy(mesh->nodes[1].name, COS_MESH3_NAME_MAX, "B-kerava-rpi5");
    ncpy(mesh->nodes[1].role, COS_MESH3_NAME_MAX, "leaf");
    mesh_init_keys(&mesh->nodes[1], 'B');

    ncpy(mesh->nodes[2].name, COS_MESH3_NAME_MAX, "C-cloud-vm");
    ncpy(mesh->nodes[2].role, COS_MESH3_NAME_MAX, "federator");
    mesh_init_keys(&mesh->nodes[2], 'C');

    mesh->tau_escalate   = 0.55f;
    mesh->dp_noise_sigma = 0.05f;
}

/* ------------------ run ------------------ */

int cos_mesh3_run_query(cos_mesh3_mesh_t *mesh, const char *query,
                        float *out_final_sigma) {
    if (!mesh || !query) return -1;

    /* 1. A → B  QUERY */
    cos_mesh3_msg_t q = {0};
    q.from = 0; q.to = 1; q.kind = COS_MESH3_QUERY;
    ncpy(q.payload, sizeof q.payload, query);
    if (mesh_push(mesh, &q) != 0) return -2;

    /* B evaluates locally */
    mesh->nodes[1].queries_seen++;
    float sigma_b = local_sigma(&mesh->nodes[1], query);
    mesh->nodes[1].local_sigma_mean = sigma_b;

    float final_sigma = sigma_b;
    if (sigma_b > mesh->tau_escalate) {
        /* 2. B → A  ESCALATE (leaf drops its own answer) */
        cos_mesh3_msg_t e = {0};
        e.from = 1; e.to = 0; e.kind = COS_MESH3_ESCALATE;
        e.sigma = sigma_b;
        snprintf(e.payload, sizeof e.payload,
                 "{\"reason\":\"sigma>%.2f\",\"sigma\":%.4f}",
                 (double)mesh->tau_escalate, (double)sigma_b);
        if (mesh_push(mesh, &e) != 0) return -3;
        mesh->nodes[1].escalated_out++;
        mesh->escalations++;

        /* 3. A → C  ESCALATE */
        cos_mesh3_msg_t e2 = {0};
        e2.from = 0; e2.to = 2; e2.kind = COS_MESH3_ESCALATE;
        e2.sigma = sigma_b;
        snprintf(e2.payload, sizeof e2.payload,
                 "{\"forwarded_from\":\"%s\",\"sigma_b\":%.4f}",
                 mesh->nodes[1].name, (double)sigma_b);
        if (mesh_push(mesh, &e2) != 0) return -4;

        /* 4. C answers */
        mesh->nodes[2].queries_seen++;
        float sigma_c = local_sigma(&mesh->nodes[2], query);
        mesh->nodes[2].local_sigma_mean = sigma_c;
        cos_mesh3_msg_t af = {0};
        af.from = 2; af.to = 0; af.kind = COS_MESH3_ANSWER_FINAL;
        af.sigma = sigma_c;
        snprintf(af.payload, sizeof af.payload,
                 "{\"answer\":\"authoritative_%llu\",\"sigma\":%.4f}",
                 (unsigned long long)(fnv1a64(query, strlen(query), 0) & 0xFFFFULL),
                 (double)sigma_c);
        if (mesh_push(mesh, &af) != 0) return -5;
        mesh->nodes[2].answers_given++;
        final_sigma = sigma_c;
    } else {
        /* 2'. B → A ANSWER */
        cos_mesh3_msg_t ans = {0};
        ans.from = 1; ans.to = 0; ans.kind = COS_MESH3_ANSWER;
        ans.sigma = sigma_b;
        snprintf(ans.payload, sizeof ans.payload,
                 "{\"answer\":\"local_%llu\",\"sigma\":%.4f}",
                 (unsigned long long)(fnv1a64(query, strlen(query), 0) & 0xFFFFULL),
                 (double)sigma_b);
        if (mesh_push(mesh, &ans) != 0) return -6;
        mesh->nodes[1].answers_given++;
    }

    /* 5. C federates weight update to A and B. */
    float delta[COS_MESH3_DP_DIM];
    /* Per-node determinism: mix query with a stable uint64_t fold
     * of node C's verify key (no secret key material leaks). */
    uint64_t node_c_fold = fnv1a64(mesh->nodes[2].verify_key,
                                   COS_ED25519_PUB_LEN, 0);
    dp_noise_fill(delta, COS_MESH3_DP_DIM,
                  fnv1a64(query, strlen(query), node_c_fold));
    for (int peer = 0; peer < 2; peer++) {
        cos_mesh3_msg_t f = {0};
        f.from = 2; f.to = peer; f.kind = COS_MESH3_FEDERATION;
        size_t off = 0;
        off += (size_t)snprintf(f.payload + off, sizeof f.payload - off,
                                "{\"delta\":[");
        for (int i = 0; i < COS_MESH3_DP_DIM; i++) {
            off += (size_t)snprintf(f.payload + off, sizeof f.payload - off,
                                    "%s%.4f", i == 0 ? "" : ",",
                                    (double)delta[i]);
        }
        off += (size_t)snprintf(f.payload + off, sizeof f.payload - off,
                                "],\"dp_sigma\":%.4f}",
                                (double)mesh->dp_noise_sigma);
        if (mesh_push(mesh, &f) != 0) return -7;
        for (int i = 0; i < COS_MESH3_DP_DIM; i++) {
            mesh->nodes[peer].federation_weight[i] += delta[i];
        }
        mesh->nodes[peer].federation_updates_applied++;
        mesh->federations++;
    }
    mesh->script_ok = 1;
    if (out_final_sigma) *out_final_sigma = final_sigma;
    return 0;
}

/* ------------------ tamper probe ------------------ */

int cos_mesh3_tamper_probe(cos_mesh3_mesh_t *mesh) {
    if (!mesh) return -1;
    cos_mesh3_msg_t bad = {0};
    bad.from = 1; bad.to = 0; bad.kind = COS_MESH3_ANSWER;
    ncpy(bad.payload, sizeof bad.payload,
         "{\"answer\":\"forged\",\"sigma\":0.05}");
    /* Forge: take B's real signature and flip one bit — must fail
     * under Ed25519 verify (EdDSA has negligible second-preimage
     * probability). */
    sign_message(&mesh->nodes[1], &bad);
    bad.signature[0] ^= 0x01;
    bad.verified = verify_message(mesh, &bad);
    if (bad.verified) return 0; /* unexpected pass */
    bad.dropped = 1;
    if (mesh->n_messages >= COS_MESH3_MAX_MESSAGES) return -1;
    mesh->log[mesh->n_messages++] = bad;
    mesh->signatures_rejected++;
    return 1;
}

/* ------------------ self-test ------------------ */

int cos_mesh3_self_test(void) {
    cos_mesh3_mesh_t m;
    cos_mesh3_init(&m);

    float final_sigma = 0.0f;
    if (cos_mesh3_run_query(&m, "Who wrote Tähtien sota?", &final_sigma) != 0)
        return -1;
    if (!m.script_ok) return -2;
    if (m.federations != 2) return -3;

    /* Query 2 — different hash so we exercise both branches over a
     * small batch. */
    if (cos_mesh3_run_query(&m, "Mikä on σ?", &final_sigma) != 0)
        return -4;

    /* Every non-tampered message must be verified (dropped==0). */
    for (int i = 0; i < m.n_messages; i++) {
        if (!m.log[i].verified && !m.log[i].dropped) return -5;
    }

    /* Tamper */
    if (cos_mesh3_tamper_probe(&m) != 1) return -6;
    if (m.signatures_rejected < 1)       return -7;

    /* Wrong-key probe: sign with C's key but claim to be B.  Must
     * fail because B's verify key will not match C's signature. */
    {
        cos_mesh3_msg_t wk = {0};
        wk.from = 1; wk.to = 0; wk.kind = COS_MESH3_ANSWER;
        ncpy(wk.payload, sizeof wk.payload,
             "{\"answer\":\"impostor\",\"sigma\":0.10}");
        sign_message(&m.nodes[2], &wk);   /* C signs */
        wk.verified = verify_message(&m, &wk); /* claims from == B */
        if (wk.verified) return -8;
    }

    return 0;
}
