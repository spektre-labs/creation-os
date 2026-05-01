/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Protocol self-test + round-trip + tamper demo. */

#include "protocol.h"

#include <stdio.h>
#include <string.h>

static void dump_hex(const uint8_t *p, size_t n, char *out, size_t cap) {
    static const char hex[] = "0123456789abcdef";
    size_t o = 0;
    size_t lim = (n > 8) ? 8 : n;    /* first 8 bytes only */
    for (size_t i = 0; i < lim && o + 3 < cap; ++i) {
        out[o++] = hex[(p[i] >> 4) & 0xF];
        out[o++] = hex[p[i] & 0xF];
    }
    if (o < cap) out[o] = '\0';
}

int main(int argc, char **argv) {
    int rc = cos_sigma_proto_self_test();

    /* Deterministic Ed25519 keypair for the demo.  sign_key
     * = priv(64) || pub(32); verify_key = pub(32).  Matches the
     * default signer (post-FNV-1a). */
    uint8_t seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i)
        seed[i] = (uint8_t)('d' + i);
    uint8_t pub[COS_ED25519_PUB_LEN];
    uint8_t priv[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);
    uint8_t sign_key[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    memcpy(sign_key, priv, COS_ED25519_PRIV_LEN);
    memcpy(sign_key + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);

    const char *question = "what is the weather in helsinki?";

    cos_msg_t tx = {
        .type         = COS_MSG_QUERY,
        .sender_id    = "node-finland",
        .sender_sigma = 0.22f,
        .timestamp_ns = 1713500000000000000ULL,
        .payload      = (const uint8_t*)question,
        .payload_len  = (uint32_t)strlen(question),
    };

    uint8_t frame[512];
    size_t  framed = 0;
    int enc = cos_sigma_proto_encode(&tx, sign_key, sizeof sign_key,
                                     frame, sizeof frame, &framed);

    cos_msg_type_t peeked = (cos_msg_type_t)0;
    int peek_rc = cos_sigma_proto_peek_type(frame, framed, &peeked);

    cos_msg_t rx;
    int dec_ok = cos_sigma_proto_decode(frame, framed, pub, COS_ED25519_PUB_LEN, &rx);

    /* Flip the sigma byte → decode must reject. */
    frame[40] ^= 0x02;
    cos_msg_t rx_bad;
    int dec_tamper = cos_sigma_proto_decode(frame, framed, pub, COS_ED25519_PUB_LEN, &rx_bad);
    frame[40] ^= 0x02;  /* restore                                  */

    /* Wrong key → decode must reject. */
    uint8_t wrong_seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i)
        wrong_seed[i] = (uint8_t)('w' + i);
    uint8_t wrong_pub[COS_ED25519_PUB_LEN];
    uint8_t wrong_priv[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(wrong_seed, wrong_pub, wrong_priv);
    cos_msg_t rx_key;
    int dec_wrong = cos_sigma_proto_decode(frame, framed, wrong_pub, COS_ED25519_PUB_LEN, &rx_key);

    char sig_head[24] = {0};
    dump_hex(frame + COS_MSG_HEADER_LEN + tx.payload_len, COS_MSG_SIG_LEN,
             sig_head, sizeof sig_head);

    printf("{\"kernel\":\"sigma_protocol\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"encode_rc\":%d,"
             "\"frame_bytes\":%zu,"
             "\"header_bytes\":%d,"
             "\"sig_bytes\":%d,"
             "\"peek_rc\":%d,"
             "\"peek_type\":\"%s\","
             "\"decode_rc\":%d,"
             "\"sender_id\":\"%s\","
             "\"sender_sigma\":%.4f,"
             "\"timestamp_ns\":%llu,"
             "\"payload_len\":%u,"
             "\"payload_hex16\":\"%c%c%c%c%c%c%c%c...\","
             "\"sig_prefix8\":\"%s\","
             "\"tamper_rc\":%d,"
             "\"wrong_key_rc\":%d"
             "},"
           "\"pass\":%s}\n",
           rc,
           enc,
           framed,
           COS_MSG_HEADER_LEN,
           COS_MSG_SIG_LEN,
           peek_rc,
           cos_sigma_proto_type_name(peeked),
           dec_ok,
           rx.sender_id,
           (double)rx.sender_sigma,
           (unsigned long long)rx.timestamp_ns,
           rx.payload_len,
           rx.payload[0], rx.payload[1], rx.payload[2], rx.payload[3],
           rx.payload[4], rx.payload[5], rx.payload[6], rx.payload[7],
           sig_head,
           dec_tamper,
           dec_wrong,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Protocol: ok decode=%d tamper=%d wrong_key=%d\n",
                dec_ok, dec_tamper, dec_wrong);
    }
    return rc;
}
