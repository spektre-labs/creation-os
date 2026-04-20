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

    uint8_t key[] = "creation-os-shared-key-v0";
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
    int enc = cos_sigma_proto_encode(&tx, key, sizeof key - 1,
                                     frame, sizeof frame, &framed);

    cos_msg_type_t peeked = (cos_msg_type_t)0;
    int peek_rc = cos_sigma_proto_peek_type(frame, framed, &peeked);

    cos_msg_t rx;
    int dec_ok = cos_sigma_proto_decode(frame, framed, key, sizeof key - 1, &rx);

    /* Flip the sigma byte → decode must reject. */
    frame[40] ^= 0x02;
    cos_msg_t rx_bad;
    int dec_tamper = cos_sigma_proto_decode(frame, framed, key, sizeof key - 1, &rx_bad);
    frame[40] ^= 0x02;  /* restore                                  */

    /* Wrong key → decode must reject. */
    uint8_t wrong[] = "impostor";
    cos_msg_t rx_key;
    int dec_wrong = cos_sigma_proto_decode(frame, framed, wrong, sizeof wrong - 1, &rx_key);

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
