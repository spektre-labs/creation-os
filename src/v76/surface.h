/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v76 — σ-Surface (the Surface kernel).
 *
 * ------------------------------------------------------------------
 *  What v76 is
 * ------------------------------------------------------------------
 *
 * v74 σ-Experience proved that the composed 15-bit stack can land
 * on a user with Apple-tier UX.  But it stopped at the surface of
 * *one* device class (the agent's own runtime).  What is still
 * missing is the single branchless integer substrate that carries
 * the entire verdict into:
 *
 *   - native iOS   (UIKit / SwiftUI / UITouch / UIAccessibility)
 *   - native Android (MotionEvent / Jetpack Compose / TalkBack)
 *   - every messenger worth bridging: WhatsApp, Telegram, Signal,
 *     iMessage, RCS, Matrix, XMPP, Discord, Slack, Line
 *   - every piece of world-important legacy software the user
 *     already depends on: Microsoft 365 (Word / Excel / PowerPoint
 *     / Outlook / Teams), Adobe (Photoshop / Illustrator /
 *     Lightroom / Premiere), AutoCAD / SolidWorks / Revit, SAP /
 *     Oracle / Salesforce, Figma / Sketch, Notion / Obsidian,
 *     Zoom / Meet / Webex, Xcode / IntelliJ / VSCode / Android
 *     Studio, Git / GitHub / GitLab, PostgreSQL / MongoDB / Redis,
 *     Stripe / QuickBooks, AWS / GCP / Azure
 *
 * without ever leaving the branchless integer + HV + XOR + popcount
 * discipline that the rest of v60..v75 already hold.
 *
 * v76 σ-Surface ships ten primitives:
 *
 *   1. Touch event decode.  UITouch / MotionEvent / pointer event
 *      → 256-bit hypervector lane-quantised on (x_q15, y_q15,
 *      pressure_q15, timestamp_q15, phase).  Deterministic.
 *
 *   2. Gesture classification.  Tap / double-tap / long-press /
 *      swipe / pinch / rotate → branchless argmin over a 6-template
 *      HV bank.  Router margin gate (d_{1} + τ ≤ d_{2}).
 *
 *   3. Haptic waveform generation.  Integer Q0.15 sine / ramp /
 *      impulse table (CoreHaptics / VibrationEffect parity) with
 *      energy-budget compare.  Branchless amplitude saturation.
 *
 *   4. Messenger protocol bridge.  10 protocols (WhatsApp /
 *      Telegram / Signal / iMessage / RCS / Matrix / XMPP /
 *      Discord / Slack / Line) normalised to a single envelope HV
 *      with explicit protocol_id lane so the same agent speaks
 *      them all.
 *
 *   5. Signal-protocol-style E2E envelope.  X3DH-style key
 *      blend → Double-Ratchet step HV using the v75 integer-only
 *      SHA-256 KDF.  Ratchet chain advances by branchless XOR
 *      bind; compromise of one step leaves siblings safe.
 *
 *   6. Accessibility (WCAG 2.2 + Apple HIG + Android Material 3).
 *      One branchless AND over:
 *          contrast_q15      ≥ 4.5 × 32768 / 21            (WCAG AA)
 *          focus_order_ok    == 1                          (monotone)
 *          motion_safe       == 1                          (reduce-motion honoured)
 *          touch_target_px  ≥ 44                           (Apple / Android guideline)
 *          label_present    == 1                           (VoiceOver / TalkBack)
 *
 *   7. Offline CRDT sync resolver.  Two LWW-register pairs + an
 *      OR-set HV merge → deterministic merged state.  One
 *      branchless `>=` + max-merge + XOR-union.  Works for
 *      WhatsApp-style multi-device delivery receipts.
 *
 *   8. Legacy-software capability template HV registry.  Bank of
 *      64 legacy-app HVs, each tagged by category (office / design
 *      / CAD / ERP / CRM / dev / DB / cloud / messenger / browser).
 *      `cos_v76_legacy_match(query_hv, tol, margin_min, out_top)`
 *      returns the nearest app with receipts.  The single integer
 *      compare becomes "this task is in Salesforce's skill set"
 *      without any LLM call.
 *
 *   9. Cross-platform file-format HV.  64 canonical formats
 *      (DOCX / XLSX / PPTX / PDF / DWG / DXF / STEP / STL / PSD /
 *      AI / SVG / PNG / JPEG / MP4 / MOV / WAV / MP3 / FLAC /
 *      JSON / XML / YAML / SQL / CSV / MD / EPUB / EML / ICS / VCF
 *      / ZIP / TAR / 7Z / …).  Given a magic-number prefix HV, the
 *      argmin over the bank in one popcount-Hamming pass tags the
 *      blob.  No libmagic.  Integer-only.
 *
 *  10. SBL — Surface Bytecode Language — 10-opcode integer ISA.
 *      Cost model (surface-units):
 *          HALT     = 1
 *          TOUCH    = 1        (decode + quantise)
 *          GESTURE  = 2        (6-template argmin + margin gate)
 *          HAPTIC   = 2        (waveform + energy compare)
 *          MSG      = 4        (protocol normalise + envelope)
 *          E2E      = 4        (X3DH mix + ratchet step)
 *          A11Y     = 2        (WCAG + HIG + Material bitmask)
 *          SYNC     = 2        (CRDT LWW + OR-set merge)
 *          LEGACY   = 4        (capability-template argmin + gate)
 *          GATE     = 1
 *
 *      GATE writes `v76_ok = 1` iff:
 *          surface_units   ≤ budget
 *          AND touch_ok    == 1
 *          AND gesture_ok  == 1
 *          AND haptic_ok   == 1
 *          AND msg_ok      == 1
 *          AND e2e_ok      == 1
 *          AND a11y_ok     == 1
 *          AND sync_ok     == 1
 *          AND legacy_ok   == 1
 *          AND abstained   == 0
 *
 * ------------------------------------------------------------------
 *  Composed 16-bit branchless decision
 * ------------------------------------------------------------------
 *
 *   v60 σ-Shield         — may the action cross?
 *   v61 Σ-Citadel        — do BLP + Biba + MLS allow the label?
 *   v62 Reasoning        — does the energy verifier close?
 *   v63 σ-Cipher         — is the sealed envelope well-formed?
 *   v64 σ-Intellect      — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex    — is the thought on-manifold + HVL cost?
 *   v66 σ-Silicon        — does the matrix path clear conformal?
 *   v67 σ-Noesis         — does the deliberation close + receipts?
 *   v68 σ-Mnemos         — recall ≥ threshold, forget ≤ budget?
 *   v69 σ-Constellation  — quorum margin ≥ τ, faults ≤ f?
 *   v70 σ-Hyperscale     — silicon + throughput + topology budget?
 *   v71 σ-Wormhole       — teleport + hop budget?
 *   v72 σ-Chain          — chain-bound step + quorum + OTS?
 *   v73 σ-Omnimodal      — Creator: flow + bind + lock + plan?
 *   v74 σ-Experience     — Experience: target + layout + world?
 *   v76 σ-Surface        — Surface: touch + gesture + msg + E2E
 *                          + a11y + sync + legacy + surface-unit
 *                          budget?
 *
 * `cos_v76_compose_decision` is a single 16-way branchless AND.
 * Nothing — no tool call, sealed message, render frame, generated
 * artefact, native iOS event, native Android event, WhatsApp /
 * Telegram / Signal / iMessage / RCS / Matrix / XMPP / Discord /
 * Slack / Line delivery, or legacy-software hand-off (Word / Excel
 * / Outlook / Photoshop / AutoCAD / SAP / Salesforce / Figma /
 * Xcode / Postgres / Stripe / AWS / …) crosses to the human or to
 * a legacy system unless every one of the sixteen kernels ALLOWs.
 *
 * (v75 σ-License is a lateral receipt-emitter: it stamps every
 *  ALLOW with a cryptographic License-Bound Receipt, it is not a
 *  gate in the compose function.  It *is* a compile-time
 *  precondition for this kernel: v76 refuses to link if the
 *  License bundle does not self-verify — see the Makefile target
 *  `check-v76` which depends on `license-check`.)
 *
 * ------------------------------------------------------------------
 *  Hardware discipline
 * ------------------------------------------------------------------
 *
 *   - HV token:   256 bits = 4 × uint64 = 32 B; naturally aligned.
 *   - All arenas: 64-B aligned via aligned_alloc.
 *   - Every "ok" predicate is a single branchless AND reduction.
 *   - No FP on any decision surface.  Ratios are Q0.15 with scale
 *     32768.  Saturating arithmetic via `int32` clamp macros.
 *   - Touch decode is a quantise + XOR into lane-specific bit
 *     positions; no conditional branches.
 *   - Gesture classify is popcount-Hamming argmin with a two-pass
 *     running-min-2 (identical pattern to v74 basis).
 *   - Haptic waveform uses a 64-entry Q0.15 sine table + integer
 *     multiply; energy = Σ w_i² clamped to int64.
 *   - Messenger bridge writes protocol_id into a dedicated byte
 *     lane of the envelope HV, XOR-binds per-protocol permutation.
 *   - E2E envelope: chain_key_next = SHA-256(chain_key || counter);
 *     message_key = SHA-256(chain_key_next).
 *   - Accessibility compares are all branchless `>=`.
 *   - CRDT merge: max over (timestamp, replica_id) breaks ties
 *     deterministically; OR-set bitset merges by `|=`.
 *   - Legacy / format registries are fixed-size 64-B aligned HV
 *     banks; argmin is a single XOR + 4 × popcount + sel_u32 loop.
 *   - 16-bit compose is a single branchless AND.
 *
 * 1 = 1.
 */

#ifndef COS_V76_SURFACE_H
#define COS_V76_SURFACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Primitive types and constants.
 * ==================================================================== */

#define COS_V76_HV_WORDS            4u          /* 4 × uint64 = 256 bits */
#define COS_V76_HV_BYTES           32u
#define COS_V76_HV_BITS           256u

typedef struct {
    uint64_t w[COS_V76_HV_WORDS];
} cos_v76_hv_t;

#define COS_V76_GESTURE_TEMPLATES   6u
#define COS_V76_HAPTIC_SAMPLES_MAX 256u
#define COS_V76_PROTOCOLS          10u
#define COS_V76_LEGACY_APPS        64u
#define COS_V76_FORMATS            64u
#define COS_V76_CRDT_ORSET_WORDS    4u          /* 256-bit OR-set */
#define COS_V76_NREGS              16u
#define COS_V76_PROG_MAX         1024u

/* Gesture ids. */
typedef enum {
    COS_V76_G_TAP          = 0,
    COS_V76_G_DOUBLE_TAP   = 1,
    COS_V76_G_LONG_PRESS   = 2,
    COS_V76_G_SWIPE        = 3,
    COS_V76_G_PINCH        = 4,
    COS_V76_G_ROTATE       = 5,
} cos_v76_gesture_t;

/* Messenger protocol ids — exact order matters (used as permutation key). */
typedef enum {
    COS_V76_P_WHATSAPP = 0,
    COS_V76_P_TELEGRAM = 1,
    COS_V76_P_SIGNAL   = 2,
    COS_V76_P_IMESSAGE = 3,
    COS_V76_P_RCS      = 4,
    COS_V76_P_MATRIX   = 5,
    COS_V76_P_XMPP     = 6,
    COS_V76_P_DISCORD  = 7,
    COS_V76_P_SLACK    = 8,
    COS_V76_P_LINE     = 9,
} cos_v76_protocol_t;

/* Legacy-app registry — ordered; the numbers are stable ABI. */
typedef enum {
    /* Microsoft 365 */
    COS_V76_APP_WORD        = 0,
    COS_V76_APP_EXCEL       = 1,
    COS_V76_APP_POWERPOINT  = 2,
    COS_V76_APP_OUTLOOK     = 3,
    COS_V76_APP_TEAMS       = 4,
    COS_V76_APP_ONEDRIVE    = 5,
    COS_V76_APP_SHAREPOINT  = 6,
    COS_V76_APP_ONENOTE     = 7,
    /* Adobe */
    COS_V76_APP_PHOTOSHOP   = 8,
    COS_V76_APP_ILLUSTRATOR = 9,
    COS_V76_APP_LIGHTROOM   = 10,
    COS_V76_APP_PREMIERE    = 11,
    COS_V76_APP_AFTEREFFECTS= 12,
    COS_V76_APP_INDESIGN    = 13,
    COS_V76_APP_ACROBAT     = 14,
    COS_V76_APP_XD          = 15,
    /* CAD / engineering */
    COS_V76_APP_AUTOCAD     = 16,
    COS_V76_APP_SOLIDWORKS  = 17,
    COS_V76_APP_REVIT       = 18,
    COS_V76_APP_FUSION360   = 19,
    COS_V76_APP_BLENDER     = 20,
    COS_V76_APP_RHINO       = 21,
    COS_V76_APP_CATIA       = 22,
    COS_V76_APP_INVENTOR    = 23,
    /* ERP / CRM / finance */
    COS_V76_APP_SAP         = 24,
    COS_V76_APP_ORACLE_ERP  = 25,
    COS_V76_APP_SALESFORCE  = 26,
    COS_V76_APP_HUBSPOT     = 27,
    COS_V76_APP_QUICKBOOKS  = 28,
    COS_V76_APP_STRIPE      = 29,
    COS_V76_APP_WORKDAY     = 30,
    COS_V76_APP_NETSUITE    = 31,
    /* Design / collab */
    COS_V76_APP_FIGMA       = 32,
    COS_V76_APP_SKETCH      = 33,
    COS_V76_APP_NOTION      = 34,
    COS_V76_APP_OBSIDIAN    = 35,
    COS_V76_APP_SLACK       = 36,
    COS_V76_APP_ZOOM        = 37,
    COS_V76_APP_MEET        = 38,
    COS_V76_APP_WEBEX       = 39,
    /* Dev tools */
    COS_V76_APP_XCODE       = 40,
    COS_V76_APP_ANDROID_STUDIO = 41,
    COS_V76_APP_VSCODE      = 42,
    COS_V76_APP_INTELLIJ    = 43,
    COS_V76_APP_GIT         = 44,
    COS_V76_APP_GITHUB      = 45,
    COS_V76_APP_GITLAB      = 46,
    COS_V76_APP_JIRA        = 47,
    /* Databases */
    COS_V76_APP_POSTGRES    = 48,
    COS_V76_APP_MYSQL       = 49,
    COS_V76_APP_MONGO       = 50,
    COS_V76_APP_REDIS       = 51,
    COS_V76_APP_SQLITE      = 52,
    COS_V76_APP_ELASTIC     = 53,
    COS_V76_APP_KAFKA       = 54,
    COS_V76_APP_SNOWFLAKE   = 55,
    /* Cloud / browsers */
    COS_V76_APP_AWS         = 56,
    COS_V76_APP_GCP         = 57,
    COS_V76_APP_AZURE       = 58,
    COS_V76_APP_CLOUDFLARE  = 59,
    COS_V76_APP_CHROME      = 60,
    COS_V76_APP_SAFARI      = 61,
    COS_V76_APP_FIREFOX     = 62,
    COS_V76_APP_EDGE        = 63,
} cos_v76_app_t;

/* Category tag — sparse coarse bucket for capability routing. */
typedef enum {
    COS_V76_CAT_OFFICE     = 0,
    COS_V76_CAT_DESIGN     = 1,
    COS_V76_CAT_CAD        = 2,
    COS_V76_CAT_ERP_CRM    = 3,
    COS_V76_CAT_COLLAB     = 4,
    COS_V76_CAT_DEV        = 5,
    COS_V76_CAT_DB         = 6,
    COS_V76_CAT_CLOUD      = 7,
    COS_V76_CAT_BROWSER    = 8,
} cos_v76_category_t;

/* File-format registry — ordered; stable ABI. */
typedef enum {
    COS_V76_FMT_DOCX = 0, COS_V76_FMT_XLSX = 1, COS_V76_FMT_PPTX = 2,
    COS_V76_FMT_PDF  = 3, COS_V76_FMT_DWG  = 4, COS_V76_FMT_DXF  = 5,
    COS_V76_FMT_STEP = 6, COS_V76_FMT_STL  = 7, COS_V76_FMT_PSD  = 8,
    COS_V76_FMT_AI   = 9, COS_V76_FMT_SVG  = 10, COS_V76_FMT_PNG = 11,
    COS_V76_FMT_JPEG = 12, COS_V76_FMT_WEBP= 13, COS_V76_FMT_HEIC= 14,
    COS_V76_FMT_MP4  = 15, COS_V76_FMT_MOV = 16, COS_V76_FMT_WEBM= 17,
    COS_V76_FMT_WAV  = 18, COS_V76_FMT_MP3 = 19, COS_V76_FMT_FLAC= 20,
    COS_V76_FMT_OGG  = 21, COS_V76_FMT_AAC = 22, COS_V76_FMT_JSON= 23,
    COS_V76_FMT_XML  = 24, COS_V76_FMT_YAML= 25, COS_V76_FMT_TOML= 26,
    COS_V76_FMT_SQL  = 27, COS_V76_FMT_CSV = 28, COS_V76_FMT_MD  = 29,
    COS_V76_FMT_EPUB = 30, COS_V76_FMT_EML = 31, COS_V76_FMT_ICS = 32,
    COS_V76_FMT_VCF  = 33, COS_V76_FMT_ZIP = 34, COS_V76_FMT_TAR = 35,
    COS_V76_FMT_GZ   = 36, COS_V76_FMT_SEVENZ = 37, COS_V76_FMT_RAR = 38,
    COS_V76_FMT_ISO  = 39, COS_V76_FMT_DMG = 40, COS_V76_FMT_APK = 41,
    COS_V76_FMT_IPA  = 42, COS_V76_FMT_DEB = 43, COS_V76_FMT_RPM = 44,
    COS_V76_FMT_ELF  = 45, COS_V76_FMT_MACHO = 46, COS_V76_FMT_PE = 47,
    COS_V76_FMT_WASM = 48, COS_V76_FMT_EXE = 49, COS_V76_FMT_JS  = 50,
    COS_V76_FMT_TS   = 51, COS_V76_FMT_PY  = 52, COS_V76_FMT_RS  = 53,
    COS_V76_FMT_GO   = 54, COS_V76_FMT_C   = 55, COS_V76_FMT_CPP = 56,
    COS_V76_FMT_JAVA = 57, COS_V76_FMT_KT  = 58, COS_V76_FMT_SWIFT = 59,
    COS_V76_FMT_SH   = 60, COS_V76_FMT_LOG = 61, COS_V76_FMT_BIN = 62,
    COS_V76_FMT_UNKNOWN = 63,
} cos_v76_format_t;

/* ====================================================================
 *  HV primitives (mirror v74 alphabet).
 * ==================================================================== */

void     cos_v76_hv_zero    (cos_v76_hv_t *dst);
void     cos_v76_hv_copy    (cos_v76_hv_t *dst, const cos_v76_hv_t *src);
void     cos_v76_hv_xor     (cos_v76_hv_t *dst, const cos_v76_hv_t *a, const cos_v76_hv_t *b);
void     cos_v76_hv_from_seed(cos_v76_hv_t *dst, uint64_t seed);
void     cos_v76_hv_permute (cos_v76_hv_t *dst, const cos_v76_hv_t *src, uint32_t k);
uint32_t cos_v76_hv_hamming (const cos_v76_hv_t *a, const cos_v76_hv_t *b);

/* ====================================================================
 *  1.  Touch decode.
 *
 *  Canonical event layout:
 *      x_q15        Q0.15 normalised horizontal (0..32767)
 *      y_q15        Q0.15 normalised vertical   (0..32767)
 *      pressure_q15 Q0.15 normalised pressure   (0..32767)
 *      timestamp_ms wall-clock millis (lower 32 bits used)
 *      phase        0=began 1=moved 2=ended 3=cancelled
 *
 *  Produces a 256-bit HV with each lane XOR-mixed into disjoint
 *  regions of the 4 words, with a per-phase permutation applied.
 * ==================================================================== */

typedef struct {
    uint16_t x_q15;
    uint16_t y_q15;
    uint16_t pressure_q15;
    uint16_t phase;          /* 0..3 */
    uint32_t timestamp_ms;
} cos_v76_touch_t;

void cos_v76_touch_decode(cos_v76_hv_t *out, const cos_v76_touch_t *t);

/* Returns 1 iff (0 ≤ x_q15 ≤ 32767) AND (0 ≤ y_q15 ≤ 32767)
 * AND (0 ≤ pressure_q15 ≤ 32767) AND (phase ≤ 3).  Branchless. */
uint8_t cos_v76_touch_ok(const cos_v76_touch_t *t);

/* ====================================================================
 *  2.  Gesture classification.
 *
 *  Fixed bank of 6 seeded template HVs.  argmin-Hamming, margin
 *  gate: d_{1} + margin_min ≤ d_{2}.
 * ==================================================================== */

typedef struct {
    cos_v76_hv_t bank[COS_V76_GESTURE_TEMPLATES];  /* seeded; read-only */
} cos_v76_gesture_bank_t;

void cos_v76_gesture_bank_init(cos_v76_gesture_bank_t *g, uint64_t seed);

typedef struct {
    uint32_t idx;          /* gesture id */
    uint32_t dist;
    uint32_t margin;
} cos_v76_gesture_result_t;

void    cos_v76_gesture_classify(const cos_v76_gesture_bank_t *g,
                                 const cos_v76_hv_t            *q,
                                 cos_v76_gesture_result_t      *out);

/* Returns 1 iff dist ≤ tol AND margin ≥ margin_min. */
uint8_t cos_v76_gesture_ok(const cos_v76_gesture_result_t *r,
                           uint32_t                        tol,
                           uint32_t                        margin_min);

/* ====================================================================
 *  3.  Haptic waveform.
 *
 *  Generates n samples into wave[] of (saturated) Q0.15 amplitudes
 *  using kind ∈ {0=sine, 1=ramp, 2=impulse}.  Returns total energy
 *  (Σ sample²) clamped to int64.  The _ok predicate gates on:
 *      n ≤ COS_V76_HAPTIC_SAMPLES_MAX
 *      AND max_abs(wave) ≤ amp_cap_q15
 *      AND energy ≤ energy_cap
 * ==================================================================== */

int64_t cos_v76_haptic_generate(int16_t *wave,
                                uint32_t n,
                                uint32_t kind,
                                uint32_t amp_q15,
                                uint32_t period);

uint8_t cos_v76_haptic_ok(const int16_t *wave,
                          uint32_t       n,
                          int32_t        amp_cap_q15,
                          int64_t        energy,
                          int64_t        energy_cap);

/* ====================================================================
 *  4.  Messenger protocol bridge.
 *
 *  envelope = permute(payload_hv, protocol_id) ⊕ recipient_hv
 *             ⊕ (sender_tag permuted by protocol_id+128)
 *  The protocol_id is re-encoded into the lowest 4 bits of w[0]
 *  so any consumer can recover it via `& 0xFu`.
 * ==================================================================== */

typedef struct {
    uint8_t            protocol_id;    /* cos_v76_protocol_t */
    const cos_v76_hv_t *payload_hv;
    const cos_v76_hv_t *recipient_hv;
    const cos_v76_hv_t *sender_hv;
} cos_v76_msg_t;

void    cos_v76_msg_encode(cos_v76_hv_t *envelope, const cos_v76_msg_t *m);
uint8_t cos_v76_msg_ok    (const cos_v76_msg_t *m);

/* Recover protocol id from an envelope HV (lowest 4 bits of w[0]). */
uint8_t cos_v76_msg_protocol(const cos_v76_hv_t *envelope);

/* ====================================================================
 *  5.  Signal-protocol-style E2E envelope.
 *
 *  X3DH-style blend:
 *      root_key_next = SHA-256(root_key || xor(dh_a, dh_b))
 *  Double-ratchet step:
 *      chain_key_next  = SHA-256(chain_key || counter_be)
 *      message_key     = SHA-256(chain_key_next)
 *
 *  All SHA-256 calls are routed to the v75 integer-only FIPS-180-4
 *  implementation via the lightweight shim below so v76 takes no
 *  additional dep.
 * ==================================================================== */

typedef struct {
    uint8_t root_key   [32];
    uint8_t chain_key  [32];
    uint64_t counter;
} cos_v76_ratchet_t;

void    cos_v76_ratchet_init(cos_v76_ratchet_t *r,
                             const uint8_t      seed[32]);

/* Advance the ratchet by one step.  Writes message_key[32]. */
void    cos_v76_ratchet_step(cos_v76_ratchet_t *r,
                             uint8_t            message_key[32]);

/* X3DH-style mix: combine two DH shared secrets into the root key. */
void    cos_v76_ratchet_mix (cos_v76_ratchet_t *r,
                             const uint8_t      dh_a[32],
                             const uint8_t      dh_b[32]);

/* Returns 1 iff the ratchet is well-formed (counter monotone,
 * chain_key not all-zero, root_key not all-zero).  Branchless. */
uint8_t cos_v76_ratchet_ok(const cos_v76_ratchet_t *r);

/* ====================================================================
 *  6.  Accessibility (WCAG 2.2 + HIG + Material 3).
 *
 *  All fields are explicit so platform bindings can marshal
 *  cleanly.  Every check is a branchless `>=`.  contrast_q15 is
 *  the foreground/background Q0.15 ratio (scaled to 32768);
 *  4.5 × 32768 / 21 ≈ 7022 is the WCAG-AA body-text threshold.
 * ==================================================================== */

typedef struct {
    uint32_t contrast_q15;
    uint8_t  focus_order_ok;     /* monotone focus ring           */
    uint8_t  motion_safe;        /* reduce-motion honoured        */
    uint16_t touch_target_px;
    uint8_t  label_present;      /* VoiceOver / TalkBack label    */
    uint8_t  pad[3];
} cos_v76_a11y_t;

uint8_t cos_v76_a11y_ok(const cos_v76_a11y_t *a);

/* ====================================================================
 *  7.  CRDT sync resolver.
 *
 *  One LWW register + one 256-bit OR-set.  Merge:
 *      (ts, rep) := (ts, rep) later-wins; ties break by rep.
 *      or_set   := a | b
 *  Deterministic.  Commutative.  Associative.  Branchless.
 * ==================================================================== */

typedef struct {
    uint64_t     timestamp;
    uint32_t     replica_id;
    uint32_t     _pad;
    uint64_t     value;                  /* LWW register payload */
    cos_v76_hv_t or_set;                 /* OR-set bitset        */
} cos_v76_crdt_t;

void    cos_v76_crdt_merge(cos_v76_crdt_t       *dst,
                           const cos_v76_crdt_t *a,
                           const cos_v76_crdt_t *b);

/* Returns 1 iff replica_id < 2^16 AND timestamp > 0. */
uint8_t cos_v76_crdt_ok(const cos_v76_crdt_t *c);

/* ====================================================================
 *  8.  Legacy-software capability-template registry.
 *
 *  64 HVs seeded deterministically from (app_id, category).
 *  Query = task HV → argmin-Hamming.  Gate: dist ≤ tol AND
 *  margin ≥ margin_min.
 * ==================================================================== */

typedef struct {
    cos_v76_hv_t bank[COS_V76_LEGACY_APPS];
    uint8_t      category[COS_V76_LEGACY_APPS];
} cos_v76_legacy_t;

void    cos_v76_legacy_init(cos_v76_legacy_t *L);

typedef struct {
    uint32_t app_id;          /* cos_v76_app_t */
    uint32_t category;        /* cos_v76_category_t */
    uint32_t dist;
    uint32_t margin;
} cos_v76_legacy_result_t;

void    cos_v76_legacy_match(const cos_v76_legacy_t  *L,
                             const cos_v76_hv_t      *query,
                             cos_v76_legacy_result_t *out);

uint8_t cos_v76_legacy_ok(const cos_v76_legacy_result_t *r,
                          uint32_t                       tol,
                          uint32_t                       margin_min);

/* ====================================================================
 *  9.  Cross-platform file-format registry.
 * ==================================================================== */

typedef struct {
    cos_v76_hv_t bank[COS_V76_FORMATS];
} cos_v76_format_bank_t;

void    cos_v76_format_bank_init(cos_v76_format_bank_t *F);

typedef struct {
    uint32_t fmt_id;       /* cos_v76_format_t */
    uint32_t dist;
    uint32_t margin;
} cos_v76_format_result_t;

void    cos_v76_format_classify(const cos_v76_format_bank_t *F,
                                const cos_v76_hv_t          *prefix_hv,
                                cos_v76_format_result_t     *out);

/* ====================================================================
 * 10.  SBL — Surface Bytecode Language — 10-opcode bytecode ISA.
 * ==================================================================== */

typedef enum {
    COS_V76_OP_HALT    = 0,
    COS_V76_OP_TOUCH   = 1,
    COS_V76_OP_GESTURE = 2,
    COS_V76_OP_HAPTIC  = 3,
    COS_V76_OP_MSG     = 4,
    COS_V76_OP_E2E     = 5,
    COS_V76_OP_A11Y    = 6,
    COS_V76_OP_SYNC    = 7,
    COS_V76_OP_LEGACY  = 8,
    COS_V76_OP_GATE    = 9,
} cos_v76_op_t;

typedef struct {
    uint8_t op;
    uint8_t dst;
    uint8_t a;
    uint8_t b;
    int16_t imm;
    uint16_t pad;
} cos_v76_sbl_inst_t;

/* Context handed to the interpreter — all pointers are borrowed. */
typedef struct {
    /* TOUCH */
    const cos_v76_touch_t  *touch;

    /* GESTURE */
    const cos_v76_gesture_bank_t *gest;
    const cos_v76_hv_t            *gest_q;
    uint32_t                       gest_tol;
    uint32_t                       gest_margin;

    /* HAPTIC */
    int16_t *haptic_wave;
    uint32_t haptic_n;
    uint32_t haptic_kind;
    uint32_t haptic_amp_q15;
    uint32_t haptic_period;
    int32_t  haptic_amp_cap_q15;
    int64_t  haptic_energy_cap;

    /* MSG */
    const cos_v76_msg_t *msg;

    /* E2E */
    cos_v76_ratchet_t *ratchet;
    uint8_t           *out_message_key; /* 32 bytes */

    /* A11Y */
    const cos_v76_a11y_t *a11y;

    /* SYNC */
    cos_v76_crdt_t       *crdt_dst;
    const cos_v76_crdt_t *crdt_a;
    const cos_v76_crdt_t *crdt_b;

    /* LEGACY */
    const cos_v76_legacy_t *legacy;
    const cos_v76_hv_t     *legacy_query;
    uint32_t                legacy_tol;
    uint32_t                legacy_margin;

    uint8_t abstain;
} cos_v76_sbl_ctx_t;

typedef struct cos_v76_sbl_state cos_v76_sbl_state_t;

cos_v76_sbl_state_t *cos_v76_sbl_new(void);
void                 cos_v76_sbl_free(cos_v76_sbl_state_t *s);
void                 cos_v76_sbl_reset(cos_v76_sbl_state_t *s);

uint64_t cos_v76_sbl_units     (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_touch_ok  (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_gesture_ok(const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_haptic_ok (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_msg_ok    (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_e2e_ok    (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_a11y_ok   (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_sync_ok   (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_legacy_ok (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_abstained (const cos_v76_sbl_state_t *s);
uint8_t  cos_v76_sbl_v76_ok    (const cos_v76_sbl_state_t *s);

/* Returns 0 on success, -1 malformed, -2 budget, -3 missing ctx. */
int cos_v76_sbl_exec(cos_v76_sbl_state_t      *s,
                     const cos_v76_sbl_inst_t *prog,
                     uint32_t                  n,
                     cos_v76_sbl_ctx_t        *ctx,
                     uint64_t                  unit_budget);

/* ====================================================================
 *  Composed 16-bit branchless decision.
 * ==================================================================== */

typedef struct {
    uint8_t v60_ok;
    uint8_t v61_ok;
    uint8_t v62_ok;
    uint8_t v63_ok;
    uint8_t v64_ok;
    uint8_t v65_ok;
    uint8_t v66_ok;
    uint8_t v67_ok;
    uint8_t v68_ok;
    uint8_t v69_ok;
    uint8_t v70_ok;
    uint8_t v71_ok;
    uint8_t v72_ok;
    uint8_t v73_ok;
    uint8_t v74_ok;
    uint8_t v76_ok;
    uint8_t allow;
} cos_v76_decision_t;

cos_v76_decision_t cos_v76_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok,
                                            uint8_t v70_ok,
                                            uint8_t v71_ok,
                                            uint8_t v72_ok,
                                            uint8_t v73_ok,
                                            uint8_t v74_ok,
                                            uint8_t v76_ok);

/* ====================================================================
 *  Version string.
 * ==================================================================== */

extern const char cos_v76_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V76_SURFACE_H */
