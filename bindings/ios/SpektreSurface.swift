// SPDX-License-Identifier: AGPL-3.0-or-later
//
// SpektreSurface.swift — thin Swift façade over v76 σ-Surface C ABI.
//
// Design goals
// ============
//   * Zero allocation on the hot path.  The C kernel already uses
//     aligned_alloc(64) internally; Swift just borrows pointers.
//   * UITouch / UIEvent interop without UIKit leakage.  We expose a
//     pure-Swift `Touch` struct that the caller is responsible for
//     populating from UITouch (a trivial 5-assignment mapping).
//   * Apple-tier usability.  Every call site reads like idiomatic
//     Swift; every verdict returns `true` iff the 16-kernel stack
//     allowed the action.
//   * WhatsApp / Telegram / Signal / iMessage / RCS / Matrix / XMPP /
//     Discord / Slack / Line ids mirror the C enum order so switch
//     statements stay exhaustive.
//
// Nothing here is thread-unsafe — the C kernel is pure + branchless
// and all state pointers are owned by the caller.

import Foundation

public enum Spektre {

    public struct Touch: Sendable {
        public var xQ15: UInt16
        public var yQ15: UInt16
        public var pressureQ15: UInt16
        public var phase: UInt16      // 0 began, 1 moved, 2 ended, 3 cancelled
        public var timestampMs: UInt32

        public init(xQ15: UInt16, yQ15: UInt16, pressureQ15: UInt16,
                    phase: UInt16, timestampMs: UInt32) {
            self.xQ15 = xQ15; self.yQ15 = yQ15
            self.pressureQ15 = pressureQ15; self.phase = phase
            self.timestampMs = timestampMs
        }

        /// Convenience from UITouch-style normalised (0..1) floats.
        public init(x: Double, y: Double, pressure: Double,
                    phase: UInt16, timestampMs: UInt32) {
            func q(_ v: Double) -> UInt16 {
                let s = max(0.0, min(1.0, v)) * 32767.0
                return UInt16(s.rounded())
            }
            self.init(xQ15: q(x), yQ15: q(y), pressureQ15: q(pressure),
                      phase: phase, timestampMs: timestampMs)
        }
    }

    public enum Protocol_: UInt8, Sendable, CaseIterable {
        case whatsApp = 0, telegram, signal, iMessage, rcs
        case matrix, xmpp, discord, slack, line
    }

    public enum Gesture: UInt32, Sendable, CaseIterable {
        case tap = 0, doubleTap, longPress, swipe, pinch, rotate
    }

    // MARK: - Touch

    public static func decode(_ t: Touch) -> (hv: (UInt64, UInt64, UInt64, UInt64), ok: Bool) {
        var raw = cos_v76_touch_t(x_q15: t.xQ15, y_q15: t.yQ15,
                                  pressure_q15: t.pressureQ15,
                                  phase: t.phase,
                                  timestamp_ms: t.timestampMs)
        var hv = cos_v76_hv_t(w: (0, 0, 0, 0))
        cos_v76_touch_decode(&hv, &raw)
        let ok = cos_v76_touch_ok(&raw) == 1
        return ((hv.w.0, hv.w.1, hv.w.2, hv.w.3), ok)
    }

    // MARK: - Composed decision (16-kernel)

    /// Returns true iff every one of the 16 kernels allows.
    public static func decide(_ bits: [UInt8]) -> Bool {
        precondition(bits.count == 16, "Spektre.decide requires 16 bits")
        let d = cos_v76_compose_decision(
            bits[0],  bits[1],  bits[2],  bits[3],
            bits[4],  bits[5],  bits[6],  bits[7],
            bits[8],  bits[9],  bits[10], bits[11],
            bits[12], bits[13], bits[14], bits[15])
        return d.allow == 1
    }

    // MARK: - Version

    public static var version: String {
        String(cString: cos_v76_version)
    }
}
