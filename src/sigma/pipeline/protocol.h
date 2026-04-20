/*
 * σ-Protocol — minimal signed binary wire format for Creation OS
 * mesh traffic (queries, responses, heartbeats, capability ads,
 * federation Δweights, engram shares, unlearn requests).
 *
 * Design axioms:
 *   1. No HTTP, no REST, no GraphQL — just length-prefixed framed
 *      binary.  v287 granite: zero external dependencies.
 *   2. Every message is signed; unsigned messages are not
 *      admissible.  The signature is pluggable (caller supplies a
 *      signer / verifier), with a deterministic 512-bit FNV-based
 *      stub shipped for self-tests and LAN-only deployments.
 *      Ed25519 can be slotted in by exposing the same interface.
 *   3. σ is first-class metadata: every message carries the
 *      sender's σ estimate for its own payload, so the receiver
 *      can gate before decoding.
 *
 * Wire layout (little-endian):
 *
 *     offset  size   field
 *     ------  -----  -----
 *      0       4     magic  = 'C','O','S',0x01
 *      4       1     type   (cos_msg_type_t)
 *      5       1     version
 *      6       2     reserved (must be 0)
 *      8      32     sender_id (zero-padded ASCII)
 *     40       4     sender_sigma (IEEE-754 float)
 *     44       8     timestamp_ns (u64)
 *     52       4     payload_len (u32)
 *     56       N     payload
 *   56+N     64      signature (over bytes 0..55+N)
 *
 * Total: 56 + N + 64 bytes.  COS_MSG_MAX_PAYLOAD guards N.
 *
 * Security model (v0):
 *   · The signer is a shared-secret HMAC-style MAC.  It is
 *     tamper-evident for bytewise flipping, unknown-key replay,
 *     wrong-sender spoofing with an honest MAC, and payload
 *     truncation.  It is NOT a substitute for asymmetric
 *     signatures — when you ship SCSL-commercial nodes, swap
 *     the signer for Ed25519 by calling cos_sigma_proto_set_
 *     signer() before handshake.
 *   · decode() verifies magic, version, reserved, payload_len
 *     bounds, and signature.  Any failure → return <0 and leave
 *     out_* untouched.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PROTOCOL_H
#define COS_SIGMA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_MSG_MAGIC_0    'C'
#define COS_MSG_MAGIC_1    'O'
#define COS_MSG_MAGIC_2    'S'
#define COS_MSG_MAGIC_3    0x01
#define COS_MSG_VERSION    1
#define COS_MSG_ID_CAP     32
#define COS_MSG_SIG_LEN    64
#define COS_MSG_HEADER_LEN 56
#define COS_MSG_FRAME_MIN  (COS_MSG_HEADER_LEN + COS_MSG_SIG_LEN)
#define COS_MSG_MAX_PAYLOAD (1024 * 1024)    /* 1 MiB cap */

typedef enum {
    COS_MSG_QUERY          = 1,
    COS_MSG_RESPONSE       = 2,
    COS_MSG_HEARTBEAT      = 3,
    COS_MSG_CAPABILITY     = 4,
    COS_MSG_FEDERATION     = 5,
    COS_MSG_ENGRAM_SHARE   = 6,
    COS_MSG_UNLEARN        = 7
} cos_msg_type_t;

typedef struct {
    cos_msg_type_t type;
    char           sender_id[COS_MSG_ID_CAP];
    float          sender_sigma;
    uint64_t       timestamp_ns;
    const uint8_t *payload;
    uint32_t       payload_len;
} cos_msg_t;

/* Signer contract: sign() writes COS_MSG_SIG_LEN bytes into `sig`,
 * deriving them from `data[0..n)` and a shared `key[]`.
 * verify() returns 1 if the signature matches, 0 otherwise. */
typedef void (*cos_sigma_proto_sign_fn)(
    const uint8_t *data, size_t n,
    const uint8_t *key,  size_t key_len,
    uint8_t       *sig);

typedef int  (*cos_sigma_proto_verify_fn)(
    const uint8_t *data, size_t n,
    const uint8_t *key,  size_t key_len,
    const uint8_t *sig);

void cos_sigma_proto_default_sign  (const uint8_t *data, size_t n,
                                    const uint8_t *key,  size_t key_len,
                                    uint8_t *sig);
int  cos_sigma_proto_default_verify(const uint8_t *data, size_t n,
                                    const uint8_t *key,  size_t key_len,
                                    const uint8_t *sig);

/* ----- Ed25519 signer (vendored orlp/ed25519, zlib license) -----
 *
 *   key packing convention for the σ-protocol signer contract:
 *
 *     sign():   key = private(64) || public(32)  → key_len must be 96
 *     verify(): key = public(32)                 → key_len must be 32
 *
 *   Signature is the native Ed25519 64-byte signature and fills the
 *   COS_MSG_SIG_LEN slot byte-for-byte (no framing).
 *
 *   Keypair generation is deterministic given a 32-byte seed; use
 *   cos_sigma_proto_ed25519_keypair_from_seed() for reproducible
 *   test keys and cos_sigma_proto_ed25519_keypair() for OS-random
 *   production keys (reads /dev/urandom on POSIX).
 */
#define COS_ED25519_SEED_LEN    32
#define COS_ED25519_PUB_LEN     32
#define COS_ED25519_PRIV_LEN    64
#define COS_ED25519_SIG_LEN     COS_MSG_SIG_LEN

void cos_sigma_proto_ed25519_sign  (const uint8_t *data, size_t n,
                                    const uint8_t *key,  size_t key_len,
                                    uint8_t *sig);
int  cos_sigma_proto_ed25519_verify(const uint8_t *data, size_t n,
                                    const uint8_t *key,  size_t key_len,
                                    const uint8_t *sig);

/* Deterministic keypair from a caller-provided 32-byte seed.
 * Returns 0 on success, <0 on bad arguments. */
int  cos_sigma_proto_ed25519_keypair_from_seed(const uint8_t *seed,
                                               uint8_t *out_pub,
                                               uint8_t *out_priv);

/* Keypair from OS RNG (/dev/urandom on POSIX).  Returns 0 on
 * success, <0 if the RNG is unavailable.  For production use. */
int  cos_sigma_proto_ed25519_keypair(uint8_t *out_pub,
                                     uint8_t *out_priv);

int  cos_sigma_proto_ed25519_self_test(void);

/* Encode `msg` into `out` using the default signer (or caller's
 * override if set).  Returns total bytes written (>0) on success,
 * <0 on argument error or insufficient capacity. */
int  cos_sigma_proto_encode(const cos_msg_t *msg,
                            const uint8_t *key, size_t key_len,
                            uint8_t *out, size_t cap,
                            size_t *out_written);

/* Decode a frame in `buf[0..n)`.  Payload pointer is aliased into
 * `buf` (no copy).  Returns 0 on success; negative on malformed
 * frame or signature mismatch. */
int  cos_sigma_proto_decode(const uint8_t *buf, size_t n,
                            const uint8_t *key, size_t key_len,
                            cos_msg_t *out_msg);

/* Inspect the type of a frame without verifying the signature —
 * useful for routing before touching the MAC. */
int  cos_sigma_proto_peek_type(const uint8_t *buf, size_t n,
                               cos_msg_type_t *out_type);

const char *cos_sigma_proto_type_name(cos_msg_type_t t);

int  cos_sigma_proto_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_PROTOCOL_H */
