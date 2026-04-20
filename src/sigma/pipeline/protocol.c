/* σ-Protocol — wire codec + default FNV-based signer. */

#include "protocol.h"

#include <string.h>

/* ---- little-endian helpers (portable, no dependency on
 *      builtin_bswap or endian.h). ---- */

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}
static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void put_u64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)((v >> (8 * i)) & 0xFF);
}
static void put_f32(uint8_t *p, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    put_u32(p, u);
}
static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)(p[0])
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint64_t get_u64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)p[i]) << (8 * i);
    return v;
}
static float get_f32(const uint8_t *p) {
    uint32_t u = get_u32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

/* ---- FNV-1a 64-bit MAC (shared-secret).  v0 stub.  Swap for
 *      Ed25519 by passing a different signer to the codec. ---- */

#define FNV64_OFFSET 0xcbf29ce484222325ULL
#define FNV64_PRIME  0x00000100000001b3ULL

static uint64_t fnv1a64_init(const uint8_t *key, size_t key_len, uint8_t round) {
    uint64_t h = FNV64_OFFSET;
    h ^= (uint64_t)round;
    h *= FNV64_PRIME;
    for (size_t i = 0; i < key_len; ++i) {
        h ^= (uint64_t)key[i];
        h *= FNV64_PRIME;
    }
    return h;
}

static uint64_t fnv1a64_update(uint64_t h, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)data[i];
        h *= FNV64_PRIME;
    }
    return h;
}

void cos_sigma_proto_default_sign(const uint8_t *data, size_t n,
                                  const uint8_t *key,  size_t key_len,
                                  uint8_t *sig)
{
    /* Eight 64-bit FNV digests, each seeded with a different round
     * number so shifting bytes changes every digest. */
    for (uint8_t r = 0; r < 8; ++r) {
        uint64_t h = fnv1a64_init(key, key_len, r);
        h = fnv1a64_update(h, data, n);
        /* Post-mix with key length + round for extra diffusion. */
        h ^= (uint64_t)n;
        h *= FNV64_PRIME;
        put_u64(sig + (r * 8), h);
    }
}

int cos_sigma_proto_default_verify(const uint8_t *data, size_t n,
                                   const uint8_t *key,  size_t key_len,
                                   const uint8_t *sig)
{
    uint8_t expected[COS_MSG_SIG_LEN];
    cos_sigma_proto_default_sign(data, n, key, key_len, expected);
    /* Constant-time compare (portable fallback). */
    unsigned diff = 0;
    for (int i = 0; i < COS_MSG_SIG_LEN; ++i) {
        diff |= (unsigned)(expected[i] ^ sig[i]);
    }
    return diff == 0;
}

/* ---- codec ---- */

int cos_sigma_proto_encode(const cos_msg_t *msg,
                           const uint8_t *key, size_t key_len,
                           uint8_t *out, size_t cap, size_t *out_written)
{
    if (!msg || !out)                       return -1;
    if (!out_written)                       return -1;
    if (msg->payload_len > 0 && !msg->payload) return -2;
    if (msg->payload_len > COS_MSG_MAX_PAYLOAD) return -3;
    if (msg->type < COS_MSG_QUERY || msg->type > COS_MSG_UNLEARN) return -4;
    size_t need = (size_t)COS_MSG_HEADER_LEN
                + (size_t)msg->payload_len
                + (size_t)COS_MSG_SIG_LEN;
    if (cap < need)                         return -5;

    uint8_t *p = out;
    p[0] = COS_MSG_MAGIC_0;
    p[1] = COS_MSG_MAGIC_1;
    p[2] = COS_MSG_MAGIC_2;
    p[3] = COS_MSG_MAGIC_3;
    p[4] = (uint8_t)msg->type;
    p[5] = COS_MSG_VERSION;
    put_u16(p + 6, 0);
    memset(p + 8, 0, COS_MSG_ID_CAP);
    {
        size_t id_len = strnlen(msg->sender_id, COS_MSG_ID_CAP);
        memcpy(p + 8, msg->sender_id, id_len);
    }
    put_f32(p + 40, msg->sender_sigma);
    put_u64(p + 44, msg->timestamp_ns);
    put_u32(p + 52, msg->payload_len);

    if (msg->payload_len > 0) {
        memcpy(p + COS_MSG_HEADER_LEN, msg->payload, msg->payload_len);
    }
    size_t signed_len = (size_t)COS_MSG_HEADER_LEN + (size_t)msg->payload_len;
    cos_sigma_proto_default_sign(p, signed_len, key, key_len,
                                 p + signed_len);
    *out_written = need;
    return 0;
}

int cos_sigma_proto_peek_type(const uint8_t *buf, size_t n,
                              cos_msg_type_t *out_type)
{
    if (!buf || n < COS_MSG_FRAME_MIN) return -1;
    if (buf[0] != COS_MSG_MAGIC_0 || buf[1] != COS_MSG_MAGIC_1 ||
        buf[2] != COS_MSG_MAGIC_2 || buf[3] != COS_MSG_MAGIC_3) return -2;
    if (buf[5] != COS_MSG_VERSION)   return -3;
    uint8_t t = buf[4];
    if (t < COS_MSG_QUERY || t > COS_MSG_UNLEARN) return -4;
    if (out_type) *out_type = (cos_msg_type_t)t;
    return 0;
}

int cos_sigma_proto_decode(const uint8_t *buf, size_t n,
                           const uint8_t *key, size_t key_len,
                           cos_msg_t *out_msg)
{
    if (!buf || !out_msg || n < COS_MSG_FRAME_MIN) return -1;
    cos_msg_type_t ty;
    int rc = cos_sigma_proto_peek_type(buf, n, &ty);
    if (rc != 0) return rc;
    if (get_u16(buf + 6) != 0) return -6;        /* reserved */
    uint32_t plen = get_u32(buf + 52);
    if (plen > COS_MSG_MAX_PAYLOAD) return -7;
    size_t need = (size_t)COS_MSG_HEADER_LEN + plen + COS_MSG_SIG_LEN;
    if (n < need)               return -8;
    size_t signed_len = (size_t)COS_MSG_HEADER_LEN + plen;
    if (!cos_sigma_proto_default_verify(buf, signed_len,
                                        key, key_len,
                                        buf + signed_len))
    {
        return -9;
    }
    memset(out_msg, 0, sizeof *out_msg);
    out_msg->type         = ty;
    memcpy(out_msg->sender_id, buf + 8, COS_MSG_ID_CAP);
    out_msg->sender_id[COS_MSG_ID_CAP - 1] = '\0';
    out_msg->sender_sigma = get_f32(buf + 40);
    out_msg->timestamp_ns = get_u64(buf + 44);
    out_msg->payload_len  = plen;
    out_msg->payload      = (plen > 0) ? (buf + COS_MSG_HEADER_LEN) : NULL;
    return 0;
}

const char *cos_sigma_proto_type_name(cos_msg_type_t t) {
    switch (t) {
    case COS_MSG_QUERY:        return "QUERY";
    case COS_MSG_RESPONSE:     return "RESPONSE";
    case COS_MSG_HEARTBEAT:    return "HEARTBEAT";
    case COS_MSG_CAPABILITY:   return "CAPABILITY";
    case COS_MSG_FEDERATION:   return "FEDERATION";
    case COS_MSG_ENGRAM_SHARE: return "ENGRAM_SHARE";
    case COS_MSG_UNLEARN:      return "UNLEARN";
    default:                   return "UNKNOWN";
    }
}

/* ---------- self-test ---------- */

static int bytes_eq(const void *a, const void *b, size_t n) {
    return memcmp(a, b, n) == 0;
}

static int check_roundtrip_query(void) {
    uint8_t key[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37 };
    const char *pl = "what is the meaning of life?";
    cos_msg_t tx = {
        .type = COS_MSG_QUERY,
        .sender_id    = "self",
        .sender_sigma = 0.23f,
        .timestamp_ns = 0xCAFEBABE12345678ULL,
        .payload      = (const uint8_t*)pl,
        .payload_len  = (uint32_t)strlen(pl),
    };
    uint8_t buf[512];
    size_t w = 0;
    if (cos_sigma_proto_encode(&tx, key, sizeof key, buf, sizeof buf, &w) != 0)
        return 10;
    if (w != (size_t)COS_MSG_HEADER_LEN + tx.payload_len + COS_MSG_SIG_LEN)
        return 11;
    cos_msg_t rx;
    if (cos_sigma_proto_decode(buf, w, key, sizeof key, &rx) != 0)
        return 12;
    if (rx.type != COS_MSG_QUERY)                         return 13;
    if (strcmp(rx.sender_id, "self") != 0)                return 14;
    if (rx.sender_sigma != tx.sender_sigma)               return 15;
    if (rx.timestamp_ns != tx.timestamp_ns)               return 16;
    if (rx.payload_len != tx.payload_len)                 return 17;
    if (!bytes_eq(rx.payload, tx.payload, rx.payload_len))return 18;
    return 0;
}

static int check_tamper(void) {
    uint8_t key[] = { 1, 2, 3, 4 };
    const uint8_t pl[] = { 'h', 'i' };
    cos_msg_t tx = {
        .type = COS_MSG_RESPONSE, .sender_id = "peer",
        .sender_sigma = 0.05f, .timestamp_ns = 1,
        .payload = pl, .payload_len = 2
    };
    uint8_t buf[256]; size_t w = 0;
    if (cos_sigma_proto_encode(&tx, key, 4, buf, sizeof buf, &w) != 0)
        return 20;
    /* Flip a byte in the payload → verify must fail. */
    buf[COS_MSG_HEADER_LEN] ^= 0x01;
    cos_msg_t rx;
    if (cos_sigma_proto_decode(buf, w, key, 4, &rx) != -9) return 21;
    /* Unflip, re-verify: must succeed again. */
    buf[COS_MSG_HEADER_LEN] ^= 0x01;
    if (cos_sigma_proto_decode(buf, w, key, 4, &rx) != 0)  return 22;
    /* Wrong key → verify must fail. */
    uint8_t wrong_key[] = { 9, 9, 9, 9 };
    if (cos_sigma_proto_decode(buf, w, wrong_key, 4, &rx) != -9) return 23;
    return 0;
}

static int check_bad_frames(void) {
    uint8_t key[] = { 1 };
    cos_msg_type_t ty;

    /* Too short. */
    uint8_t tiny[16] = {0};
    if (cos_sigma_proto_peek_type(tiny, 16, &ty) != -1) return 30;

    /* Bad magic. */
    uint8_t buf[COS_MSG_FRAME_MIN];
    memset(buf, 0, sizeof buf);
    buf[0] = 'X'; buf[1] = 'X'; buf[2] = 'X'; buf[3] = 0x02;
    if (cos_sigma_proto_peek_type(buf, sizeof buf, &ty) != -2) return 31;

    /* Bad version. */
    buf[0] = COS_MSG_MAGIC_0; buf[1] = COS_MSG_MAGIC_1;
    buf[2] = COS_MSG_MAGIC_2; buf[3] = COS_MSG_MAGIC_3;
    buf[4] = COS_MSG_QUERY;   buf[5] = 42;
    if (cos_sigma_proto_peek_type(buf, sizeof buf, &ty) != -3) return 32;

    /* Invalid type. */
    buf[5] = COS_MSG_VERSION;
    buf[4] = 99;
    if (cos_sigma_proto_peek_type(buf, sizeof buf, &ty) != -4) return 33;

    /* Encode rejects oversize payload_len. */
    cos_msg_t big = {
        .type = COS_MSG_QUERY, .sender_id = "x",
        .payload_len = COS_MSG_MAX_PAYLOAD + 1,
        .payload     = (const uint8_t*)"dummy",
    };
    uint8_t small[64]; size_t w = 0;
    if (cos_sigma_proto_encode(&big, key, 1, small, sizeof small, &w) != -3)
        return 34;
    return 0;
}

static int check_all_types(void) {
    uint8_t key[] = { 7, 7, 7 };
    const cos_msg_type_t types[] = {
        COS_MSG_QUERY, COS_MSG_RESPONSE, COS_MSG_HEARTBEAT,
        COS_MSG_CAPABILITY, COS_MSG_FEDERATION,
        COS_MSG_ENGRAM_SHARE, COS_MSG_UNLEARN
    };
    uint8_t buf[256];
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i) {
        cos_msg_t tx = {
            .type = types[i], .sender_id = "id",
            .sender_sigma = 0.5f, .timestamp_ns = (uint64_t)i,
            .payload = (const uint8_t*)"ok", .payload_len = 2
        };
        size_t w = 0;
        if (cos_sigma_proto_encode(&tx, key, 3, buf, sizeof buf, &w) != 0)
            return 40 + (int)i;
        cos_msg_t rx;
        if (cos_sigma_proto_decode(buf, w, key, 3, &rx) != 0)
            return 50 + (int)i;
        if (rx.type != types[i])
            return 60 + (int)i;
    }
    return 0;
}

int cos_sigma_proto_self_test(void) {
    int rc;
    if ((rc = check_roundtrip_query())) return rc;
    if ((rc = check_tamper()))          return rc;
    if ((rc = check_bad_frames()))      return rc;
    if ((rc = check_all_types()))       return rc;
    return 0;
}
