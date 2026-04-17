// SPDX-License-Identifier: AGPL-3.0-or-later
//
// SpektreSurface.kt — Kotlin façade over v76 σ-Surface on Android.
//
// Design goals
// ============
//   * Zero Android framework dependency in the library.  Callers
//     adapt MotionEvent → Touch themselves (a 5-line mapping).
//   * Safe `@JvmStatic` entry points so Java code can call without
//     Kotlin interop.
//   * Every API returns a deterministic verdict from the C kernel;
//     no JVM allocation on the hot path once the JNI library is
//     loaded.
//
// Usage:
//   val v = SpektreSurface.version
//   val (hv, ok) = SpektreSurface.decode(Touch(0.5, 0.5, 0.8, phase=1, timestampMs=System.currentTimeMillis()))
//   val allow = SpektreSurface.decide(bits16)

package dev.spektre.surface

object SpektreSurface {
    init { System.loadLibrary("spektre_surface") }

    data class Touch(
        val xQ15: Int,
        val yQ15: Int,
        val pressureQ15: Int,
        val phase: Int,
        val timestampMs: Long,
    ) {
        constructor(x: Double, y: Double, pressure: Double,
                    phase: Int, timestampMs: Long) :
            this(
                xQ15        = (x.coerceIn(0.0, 1.0)        * 32767.0).toInt(),
                yQ15        = (y.coerceIn(0.0, 1.0)        * 32767.0).toInt(),
                pressureQ15 = (pressure.coerceIn(0.0, 1.0) * 32767.0).toInt(),
                phase       = phase,
                timestampMs = timestampMs,
            )
    }

    data class Hv(val w0: Long, val w1: Long, val w2: Long, val w3: Long)

    val version: String get() = nativeVersion()

    fun decode(t: Touch): Pair<Hv, Boolean> {
        val words = nativeTouchDecode(t.xQ15, t.yQ15, t.pressureQ15, t.phase, t.timestampMs)
        val ok = nativeTouchOk(t.xQ15, t.yQ15, t.pressureQ15, t.phase, t.timestampMs)
        return Hv(words[0], words[1], words[2], words[3]) to ok
    }

    /**
     * Composed 16-bit decision.  [bits] must contain exactly 16 elements,
     * each 0 or 1, in kernel order: v60, v61, v62, v63, v64, v65, v66,
     * v67, v68, v69, v70, v71, v72, v73, v74, v76.
     */
    fun decide(bits: ByteArray): Boolean {
        require(bits.size == 16) { "decide requires 16 bits" }
        return nativeDecide(bits)
    }

    @JvmStatic external fun nativeVersion(): String
    @JvmStatic external fun nativeTouchOk(
        xQ15: Int, yQ15: Int, pressureQ15: Int, phase: Int, timestampMs: Long): Boolean
    @JvmStatic external fun nativeTouchDecode(
        xQ15: Int, yQ15: Int, pressureQ15: Int, phase: Int, timestampMs: Long): LongArray
    @JvmStatic external fun nativeDecide(bits: ByteArray): Boolean
}
