/*
 * creation_os_sigma_ed25519 — exercise the σ-protocol Ed25519 signer.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "protocol.h"

#include <stdio.h>
#include <string.h>

static void print_hex(const char *label, const uint8_t *b, size_t n) {
    printf("  \"%s\":\"", label);
    for (size_t i = 0; i < n; ++i) printf("%02x", b[i]);
    printf("\"");
}

int main(void) {
    int rc = cos_sigma_proto_ed25519_self_test();
    printf("{\n");
    printf("  \"tool\":\"creation_os_sigma_ed25519\",\n");
    printf("  \"self_test_rc\":%d,\n", rc);
    printf("  \"pass\":%s,\n", rc == 0 ? "true" : "false");

    /* Deterministic demo keypair from fixed seed, stable across runs. */
    unsigned char seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i) seed[i] = (unsigned char)i;
    uint8_t pub[COS_ED25519_PUB_LEN], priv[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);

    uint8_t sk[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    memcpy(sk, priv, COS_ED25519_PRIV_LEN);
    memcpy(sk + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);

    const uint8_t msg[] = "creation-os:sigma-gate";
    uint8_t sig[COS_ED25519_SIG_LEN];
    cos_sigma_proto_ed25519_sign(msg, sizeof msg - 1, sk, sizeof sk, sig);

    int verified = cos_sigma_proto_ed25519_verify(msg, sizeof msg - 1,
                                                  pub, COS_ED25519_PUB_LEN, sig);
    /* Tamper → reject. */
    uint8_t tampered[sizeof msg - 1];
    memcpy(tampered, msg, sizeof tampered);
    tampered[0] ^= 0x01;
    int rejected_tamper = !cos_sigma_proto_ed25519_verify(tampered, sizeof tampered,
                                                          pub, COS_ED25519_PUB_LEN, sig);
    /* Wrong key → reject. */
    unsigned char seed2[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i) seed2[i] = (unsigned char)(0xA5 ^ i);
    uint8_t pub2[COS_ED25519_PUB_LEN], priv2[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed2, pub2, priv2);
    int rejected_wrongkey = !cos_sigma_proto_ed25519_verify(msg, sizeof msg - 1,
                                                            pub2, COS_ED25519_PUB_LEN, sig);

    printf("  \"demo\":{\n");
    print_hex("seed",   seed, COS_ED25519_SEED_LEN); printf(",\n");
    print_hex("pub",    pub,  COS_ED25519_PUB_LEN);  printf(",\n");
    print_hex("signature", sig, COS_ED25519_SIG_LEN); printf(",\n");
    printf("    \"verified\":%s,\n",         verified         ? "true" : "false");
    printf("    \"tamper_rejected\":%s,\n",  rejected_tamper  ? "true" : "false");
    printf("    \"wrongkey_rejected\":%s\n", rejected_wrongkey? "true" : "false");
    printf("  }\n");
    printf("}\n");
    return rc;
}
