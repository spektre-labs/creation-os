/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * cos network — distributed CLI (D6).
 *
 * Composes the D-series primitives (σ-Mesh, σ-Split,
 * σ-Marketplace, σ-Federation, σ-Protocol) into a single
 * command-line surface:
 *
 *   cos network join                        — bootstrap a peer
 *                                              registry.
 *   cos network list                         — dump the registry.
 *   cos network status                       — σ per node + running
 *                                              spend + ban floor.
 *   cos network serve [--price <eur>]        — advertise local
 *                                              capacity as a
 *                                              marketplace provider.
 *   cos network query "question"             — route a query:
 *                                              mesh peer first,
 *                                              marketplace fallback,
 *                                              abstain if nobody
 *                                              clears σ budget.
 *   cos network federate                     — run a σ-weighted Δw
 *                                              aggregation round.
 *   cos network unlearn "data"               — broadcast a signed
 *                                              UNLEARN frame (GDPR).
 *   cos network (no args | --help)           — banner + usage.
 *
 * Deterministic: the CLI operates on a pre-baked state (4 mesh
 * peers + 3 marketplace providers + a 3-node federation cohort)
 * so each subcommand emits a pinnable JSON receipt.
 *
 * Flags (global):
 *   --json                 — emit a single JSON object per call.
 *   --scsl-only            — refuse non-SCSL providers (default).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "mesh.h"
#include "split.h"
#include "marketplace.h"
#include "federation.h"
#include "protocol.h"
#include "dp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- canonical bootstrapped state ---- */

static void bootstrap_mesh(cos_mesh_t *m, cos_mesh_node_t *slots, int cap) {
    cos_sigma_mesh_init(m, slots, cap,
                        0.70f, 0.30f, 100.0f, 0.80f, 0.80f);
    cos_sigma_mesh_register(m, "self",   "127.0.0.1:7000", 1, 0.30f,   0.0f);
    cos_sigma_mesh_register(m, "strong", "10.0.0.2:7000",  0, 0.10f,  40.0f);
    cos_sigma_mesh_register(m, "slow",   "10.0.0.3:7000",  0, 0.15f, 300.0f);
    cos_sigma_mesh_register(m, "bad",    "10.0.0.4:7000",  0, 0.90f,  20.0f);
}

static void bootstrap_market(cos_mkt_t *k, cos_mkt_provider_t *slots, int cap) {
    cos_sigma_mkt_init(k, slots, cap, 0.60f, 0.80f);
    cos_sigma_mkt_register(k, "cheap", "m-small",  0.002, 0.25f, 120.0f, 1);
    cos_sigma_mkt_register(k, "mid",   "m-medium", 0.008, 0.12f,  80.0f, 1);
    cos_sigma_mkt_register(k, "pro",   "m-large",  0.020, 0.08f,  60.0f, 1);
}

/* ---- subcommand handlers ---- */

static int cmd_join(int json) {
    cos_mesh_t     mesh;  cos_mesh_node_t     nodes[8];
    cos_mkt_t      mkt;   cos_mkt_provider_t  prov [8];
    bootstrap_mesh  (&mesh, nodes, 8);
    bootstrap_market(&mkt,  prov,  8);
    if (json) {
        printf("{\"command\":\"join\",\"peers\":%d,\"providers\":%d,"
               "\"self\":\"self\",\"status\":\"joined\"}\n",
               mesh.count, mkt.count);
    } else {
        printf("cos network: joined mesh (%d peers, %d providers)\n",
               mesh.count, mkt.count);
    }
    return 0;
}

static int cmd_list(int json) {
    cos_mesh_t     mesh;  cos_mesh_node_t     nodes[8];
    bootstrap_mesh(&mesh, nodes, 8);
    if (json) {
        printf("{\"command\":\"list\",\"peers\":[");
        for (int i = 0; i < mesh.count; ++i) {
            printf("%s{\"id\":\"%s\",\"address\":\"%s\",\"sigma\":%.2f,"
                   "\"latency_ms\":%.1f,\"available\":%s,\"self\":%s}",
                   i == 0 ? "" : ",",
                   nodes[i].node_id,
                   nodes[i].address,
                   (double)nodes[i].sigma_capability,
                   (double)nodes[i].latency_ms,
                   nodes[i].available ? "true" : "false",
                   nodes[i].is_self  ? "true" : "false");
        }
        printf("]}\n");
    } else {
        for (int i = 0; i < mesh.count; ++i) {
            printf("  %-8s %-18s σ=%.2f lat=%.0fms %s%s\n",
                   nodes[i].node_id,
                   nodes[i].address,
                   (double)nodes[i].sigma_capability,
                   (double)nodes[i].latency_ms,
                   nodes[i].is_self ? "[self] " : "",
                   nodes[i].available ? "up" : "down");
        }
    }
    return 0;
}

static int cmd_status(int json) {
    cos_mesh_t     mesh;  cos_mesh_node_t     nodes[8];
    cos_mkt_t      mkt;   cos_mkt_provider_t  prov [8];
    bootstrap_mesh  (&mesh, nodes, 8);
    bootstrap_market(&mkt,  prov,  8);
    /* Simulate a few reports so status has something to show. */
    cos_sigma_mesh_report(&mesh,   "strong", 0.05f, 30.0f, 0.001, 1);
    cos_sigma_mesh_report(&mesh,   "slow",   0.20f, 250.0f, 0.001, 1);
    cos_sigma_mkt_report (&mkt,    "pro",    0.05f, 55.0f,  0.020, 1);

    float overall = 0.0f;
    int   ok = 0;
    for (int i = 0; i < mesh.count; ++i) {
        if (nodes[i].available && !nodes[i].is_self) {
            overall += nodes[i].sigma_capability;
            ok++;
        }
    }
    float mean = (ok > 0) ? overall / (float)ok : 1.0f;

    if (json) {
        printf("{\"command\":\"status\","
               "\"mean_peer_sigma\":%.4f,"
               "\"peers_available\":%d,"
               "\"total_spend_eur\":%.4f,"
               "\"abstain_floor\":%.2f,"
               "\"ban_floor\":%.2f}\n",
               (double)mean,
               ok,
               mkt.total_spend_eur,
               (double)mesh.tau_abstain,
               (double)mkt.sigma_ban_floor);
    } else {
        printf("mesh: %d peers, mean σ=%.4f; spend=%.4f€\n",
               ok, (double)mean, mkt.total_spend_eur);
    }
    return 0;
}

static int cmd_serve(int json, double price) {
    cos_mkt_t mkt;  cos_mkt_provider_t prov[8];
    bootstrap_market(&mkt, prov, 8);
    /* Register self as a provider at the given price. */
    int rc = cos_sigma_mkt_register(&mkt, "self", "cos-local",
                                    price, 0.20f, 40.0f, 1);
    if (json) {
        printf("{\"command\":\"serve\",\"price_eur\":%.4f,"
               "\"provider_id\":\"self\",\"scsl_accepted\":true,"
               "\"rc\":%d,\"providers\":%d}\n",
               price, rc, mkt.count);
    } else {
        printf("cos network serve: advertising self @ %.4f€/query\n", price);
    }
    return 0;
}

static int cmd_query(int json, const char *question) {
    if (!question || !*question) {
        fprintf(stderr, "cos network query: missing question\n");
        return 2;
    }
    cos_mesh_t     mesh;  cos_mesh_node_t     nodes[8];
    cos_mkt_t      mkt;   cos_mkt_provider_t  prov [8];
    bootstrap_mesh  (&mesh, nodes, 8);
    bootstrap_market(&mkt,  prov,  8);

    /* Assume local σ is high (0.90) so the kernel punts to the
     * mesh first; if the mesh abstains, the caller tries the
     * marketplace.  */
    const cos_mesh_node_t *peer =
        cos_sigma_mesh_route(&mesh, 0.90f, 0.30f, 0);
    const char *route     = "none";
    const char *picked_id = "(none)";
    double      price     = 0.0;
    float       sigma_exp = 1.0f;

    if (peer) {
        route     = "mesh";
        picked_id = peer->node_id;
        sigma_exp = peer->sigma_capability;
        price     = 0.001;                  /* neighbour tier */
    } else {
        const cos_mkt_provider_t *p =
            cos_sigma_mkt_select(&mkt, 0.05, 0.15f, 200.0f);
        if (p) {
            route     = "marketplace";
            picked_id = p->provider_id;
            sigma_exp = p->mean_sigma;
            price     = p->price_per_query;
        }
    }

    if (strcmp(route, "none") == 0) {
        if (json) {
            printf("{\"command\":\"query\",\"question\":\"%s\","
                   "\"route\":\"abstain\",\"reason\":"
                   "\"no_peer_under_sigma_budget\"}\n",
                   question);
        } else {
            printf("cos network query: ABSTAIN (no route under σ budget)\n");
        }
        return 0;
    }

    if (json) {
        printf("{\"command\":\"query\",\"question\":\"%s\","
               "\"route\":\"%s\",\"peer\":\"%s\","
               "\"expected_sigma\":%.4f,\"price_eur\":%.4f}\n",
               question, route, picked_id, (double)sigma_exp, price);
    } else {
        printf("cos network query: %s → %s (σ=%.2f, %.4f€)\n",
               route, picked_id, (double)sigma_exp, price);
    }
    return 0;
}

static int cmd_federate(int json) {
    cos_fed_t         fed;
    cos_fed_update_t  slots[4];
    cos_sigma_fed_init(&fed, slots, 4, 0.70f, 3.0f);
    float d1[3] = { 0.10f, 0.05f, 0.02f };
    float d2[3] = { 0.12f, 0.06f, 0.03f };
    float d3[3] = { 0.20f, 0.10f, 0.05f };
    cos_sigma_fed_add(&fed, "trusted-1", d1, 3, 0.10f, 200);
    cos_sigma_fed_add(&fed, "trusted-2", d2, 3, 0.15f, 150);
    cos_sigma_fed_add(&fed, "drift",     d3, 3, 0.30f, 100);

    float global[3];
    cos_fed_report_t rep;
    cos_sigma_fed_aggregate(&fed, global, &rep);

    if (json) {
        printf("{\"command\":\"federate\","
               "\"updates\":%d,"
               "\"accepted\":%d,"
               "\"rejected_sigma\":%d,"
               "\"rejected_poison\":%d,"
               "\"total_weight\":%.2f,"
               "\"delta\":[%.4f,%.4f,%.4f],"
               "\"delta_l2\":%.4f}\n",
               fed.count, rep.n_accepted,
               rep.n_rejected_sigma, rep.n_rejected_poison,
               (double)rep.total_weight,
               (double)global[0], (double)global[1], (double)global[2],
               (double)rep.delta_l2_norm);
    } else {
        printf("cos network federate: accepted=%d, Δ‖·‖₂=%.4f\n",
               rep.n_accepted, (double)rep.delta_l2_norm);
    }
    return 0;
}

static int cmd_unlearn(int json, const char *data) {
    if (!data || !*data) {
        fprintf(stderr, "cos network unlearn: missing data\n");
        return 2;
    }
    /* Deterministic Ed25519 keypair derived from a fixed seed, so the
     * CLI command is byte-reproducible across hosts.  In production
     * the operator supplies a per-node keypair instead. */
    uint8_t seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i)
        seed[i] = (uint8_t)('u' + i);
    uint8_t pub[COS_ED25519_PUB_LEN];
    uint8_t priv[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);
    uint8_t sk[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    memcpy(sk, priv, COS_ED25519_PRIV_LEN);
    memcpy(sk + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);

    cos_msg_t tx = {
        .type         = COS_MSG_UNLEARN,
        .sender_sigma = 0.05f,     /* the request itself is trusted */
        .timestamp_ns = 1713600000000000000ULL,
        .payload      = (const uint8_t*)data,
        .payload_len  = (uint32_t)strlen(data),
    };
    strncpy(tx.sender_id, "self", COS_MSG_ID_CAP - 1);
    uint8_t frame[1024]; size_t w = 0;
    int enc = cos_sigma_proto_encode(&tx, sk, sizeof sk,
                                     frame, sizeof frame, &w);
    cos_msg_t rx;
    int dec = cos_sigma_proto_decode(frame, w, pub, COS_ED25519_PUB_LEN, &rx);

    if (json) {
        printf("{\"command\":\"unlearn\",\"data\":\"%s\","
               "\"encode_rc\":%d,\"frame_bytes\":%zu,"
               "\"type\":\"%s\",\"decode_rc\":%d,"
               "\"verified\":%s}\n",
               data, enc, w,
               cos_sigma_proto_type_name(rx.type),
               dec, (dec == 0) ? "true" : "false");
    } else {
        printf("cos network unlearn: broadcast %zu-byte signed UNLEARN for \"%s\"\n",
               w, data);
    }
    return 0;
}

static int cmd_keygen(int json, const char *seed_hex) {
    /* Generate (or re-derive) an Ed25519 keypair for this node's
     * σ-Protocol signer.  Default path: ~/.cos/node_{key,pub}.
     * If --seed <hex64> is supplied, the keypair is derived
     * deterministically from that 32-byte seed (reproducible
     * across runs and hosts).  Otherwise /dev/urandom is used. */
    uint8_t pub[COS_ED25519_PUB_LEN], priv[COS_ED25519_PRIV_LEN];
    uint8_t seed[COS_ED25519_SEED_LEN];
    int deterministic = 0;
    int rc = 0;

    if (seed_hex) {
        if (strlen(seed_hex) != (size_t)(COS_ED25519_SEED_LEN * 2)) {
            fprintf(stderr, "cos network keygen: --seed requires 64 hex chars\n");
            return 2;
        }
        for (int i = 0; i < COS_ED25519_SEED_LEN; ++i) {
            unsigned v;
            if (sscanf(seed_hex + 2 * i, "%2x", &v) != 1) {
                fprintf(stderr, "cos network keygen: bad hex at byte %d\n", i);
                return 2;
            }
            seed[i] = (uint8_t)v;
        }
        rc = cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);
        deterministic = 1;
    } else {
        rc = cos_sigma_proto_ed25519_keypair(pub, priv);
    }
    if (rc != 0) {
        fprintf(stderr, "cos network keygen: keypair generation failed rc=%d\n", rc);
        return 1;
    }

    /* Persist to ~/.cos/ unless we are in deterministic mode for
     * JSON-only self-tests (where we explicitly don't want to
     * touch the user's real keyring). */
    const char *home = getenv("HOME");
    char keydir[512], privpath[512], pubpath[512];
    int wrote = 0;
    if (home && !deterministic) {
        snprintf(keydir,   sizeof keydir,   "%s/.cos", home);
        snprintf(privpath, sizeof privpath, "%s/.cos/node_key", home);
        snprintf(pubpath,  sizeof pubpath,  "%s/.cos/node_pub", home);
        /* mkdir -p best-effort; we keep persistence optional. */
        #ifdef _WIN32
        #include <direct.h>
        (void)_mkdir(keydir);
        #else
        #include <sys/stat.h>
        mkdir(keydir, 0700);
        #endif
        FILE *fp = fopen(privpath, "wb");
        FILE *fq = fopen(pubpath,  "wb");
        if (fp && fq) {
            fwrite(priv, 1, sizeof priv, fp);
            fwrite(pub,  1, sizeof pub,  fq);
            wrote = 1;
        }
        if (fp) fclose(fp);
        if (fq) fclose(fq);
    }

    if (json) {
        printf("{\"command\":\"keygen\",\"algorithm\":\"ed25519\","
               "\"deterministic\":%s,\"persisted\":%s,"
               "\"pub_len\":%d,\"priv_len\":%d,\"pub\":\"",
               deterministic ? "true" : "false",
               wrote         ? "true" : "false",
               COS_ED25519_PUB_LEN, COS_ED25519_PRIV_LEN);
        for (int i = 0; i < COS_ED25519_PUB_LEN; ++i) printf("%02x", pub[i]);
        printf("\"}\n");
    } else {
        printf("cos network keygen: Ed25519 keypair generated (%s)\n",
               deterministic ? "deterministic from --seed" : "from /dev/urandom");
        if (wrote) {
            printf("  private: %s\n", privpath);
            printf("  public : %s\n", pubpath);
        } else if (deterministic) {
            printf("  (deterministic mode — keys not persisted)\n");
        }
        printf("  pub: ");
        for (int i = 0; i < COS_ED25519_PUB_LEN; ++i) printf("%02x", pub[i]);
        printf("\n");
    }
    return 0;
}

static int cmd_dp_status(int json) {
    /* PROD-1: σ-DP budget introspection.  We bootstrap a canonical
     * DP state (ε=0.5/round, ε_total=10.0, clip=1.0, seed fixed),
     * apply three rounds of representative gradients, and surface
     * the budget + σ_dp trend.  The emitted shape is stable so
     * Prometheus scrapers can reduce it to cos_dp_eps_spent /
     * cos_dp_eps_remaining / cos_dp_sigma_impact labels. */
    cos_sigma_dp_state_t st;
    cos_sigma_dp_init(&st, /*ε=*/0.5f, /*δ=*/1e-6f,
                           /*clip=*/1.0f, /*ε_total=*/10.0f,
                           /*seed=*/0xC05D0D90ULL);

    float rounds[3][4] = {
        { 0.40f, -0.20f,  0.10f,  0.05f },
        { 0.30f,  0.15f, -0.10f,  0.05f },
        { 0.50f, -0.30f,  0.20f,  0.10f },
    };
    cos_sigma_dp_report_t rep[3];
    for (int r = 0; r < 3; r++)
        cos_sigma_dp_add_noise(&st, rounds[r], 4, &rep[r]);

    float sigma_mean = (rep[0].sigma_dp + rep[1].sigma_dp + rep[2].sigma_dp) / 3.0f;
    int   rounds_left = cos_sigma_dp_rounds_left(&st);
    float pct_spent   = 100.0f * (st.epsilon_spent / st.epsilon_total);

    if (json) {
        printf("{\"command\":\"dp-status\","
               "\"epsilon_per_round\":%.4f,"
               "\"delta\":%.2e,"
               "\"clip_norm\":%.4f,"
               "\"epsilon_spent\":%.4f,"
               "\"epsilon_total\":%.4f,"
               "\"epsilon_remaining\":%.4f,"
               "\"pct_spent\":%.1f,"
               "\"rounds_applied\":%d,"
               "\"rounds_left\":%d,"
               "\"sigma_impact_mean\":%.4f,"
               "\"sigma_impact_last\":%.4f}\n",
               (double)st.epsilon,
               (double)st.delta,
               (double)st.clip_norm,
               (double)st.epsilon_spent,
               (double)st.epsilon_total,
               (double)cos_sigma_dp_remaining(&st),
               (double)pct_spent,
               st.rounds,
               rounds_left,
               (double)sigma_mean,
               (double)rep[2].sigma_dp);
    } else {
        printf("cos network dp-status:\n");
        printf("  ε_spent        %.2f / %.2f  (%.0f%% used)\n",
               (double)st.epsilon_spent, (double)st.epsilon_total, (double)pct_spent);
        printf("  δ              %.2e\n", (double)st.delta);
        printf("  clip_norm      %.2f\n", (double)st.clip_norm);
        printf("  rounds applied %d, rounds left %d\n", st.rounds, rounds_left);
        printf("  σ_impact       +%.4f (mean across last %d rounds)\n",
               (double)sigma_mean, st.rounds);
    }
    return 0;
}

static int usage(int rc) {
    puts("usage: cos network <subcommand> [options]");
    puts("  join                        join the mesh (bootstrap peers)");
    puts("  keygen [--seed <hex64>]     generate an Ed25519 σ-Protocol keypair");
    puts("  list                        list mesh peers");
    puts("  status                      σ per peer + marketplace spend");
    puts("  serve [--price <eur>]       advertise local capacity");
    puts("  query \"<question>\"          route a question (mesh → market → abstain)");
    puts("  federate                    run a σ-weighted Δweight aggregation");
    puts("  dp-status                   differential-privacy ε-budget + σ_impact");
    puts("  unlearn \"<data>\"            broadcast a signed UNLEARN frame");
    puts("  --json                      emit JSON receipt");
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 ||
                    strcmp(argv[1], "-h") == 0) {
        return usage(argc < 2 ? 2 : 0);
    }
    int   json = 0;
    double price = 0.01;
    const char *sub = argv[1];
    const char *query_text = NULL;
    const char *unlearn_text = NULL;
    const char *seed_hex = NULL;

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0) { json = 1; continue; }
        if (strcmp(argv[i], "--price") == 0 && i + 1 < argc) {
            price = strtod(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed_hex = argv[++i];
            continue;
        }
        /* Positional argument for query / unlearn. */
        if ((strcmp(sub, "query") == 0 || strcmp(sub, "unlearn") == 0) &&
            argv[i][0] != '-')
        {
            if (strcmp(sub, "query") == 0 && !query_text)
                query_text = argv[i];
            else if (strcmp(sub, "unlearn") == 0 && !unlearn_text)
                unlearn_text = argv[i];
        }
    }

    if (strcmp(sub, "join")     == 0) return cmd_join    (json);
    if (strcmp(sub, "keygen")   == 0) return cmd_keygen  (json, seed_hex);
    if (strcmp(sub, "list")     == 0) return cmd_list    (json);
    if (strcmp(sub, "status")   == 0) return cmd_status  (json);
    if (strcmp(sub, "serve")    == 0) return cmd_serve   (json, price);
    if (strcmp(sub, "query")    == 0) return cmd_query   (json, query_text);
    if (strcmp(sub, "federate")  == 0) return cmd_federate (json);
    if (strcmp(sub, "dp-status") == 0) return cmd_dp_status(json);
    if (strcmp(sub, "unlearn")   == 0) return cmd_unlearn  (json, unlearn_text);

    fprintf(stderr, "cos network: unknown subcommand \"%s\"\n", sub);
    return usage(2);
}
