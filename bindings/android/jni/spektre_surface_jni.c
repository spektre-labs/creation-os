/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * spektre_surface_jni.c — JNI shim for v76 σ-Surface.
 *
 * Consumed by the Kotlin / Java façade `SpektreSurface.kt`.  No
 * Android framework deps on the hot path — the shim is a thin
 * pointer-marshalling layer over the libc-only C kernel.
 *
 * Build (from NDK):
 *
 *   $NDK/toolchains/llvm/prebuilt/<host>/bin/clang \
 *     -O2 -fPIC -std=c11 -shared \
 *     -Iapp/src/main/jni -I../../src/v76 -I../../src/license_kernel \
 *     -o libspektre_surface.so \
 *     app/src/main/jni/spektre_surface_jni.c \
 *     ../../src/v76/surface.c \
 *     ../../src/license_kernel/license_attest.c \
 *     -llog
 *
 * or let Gradle's CMake plugin drive it via `bindings/android/jni/CMakeLists.txt`.
 */

#include <jni.h>
#include <string.h>

#include "../../src/v76/surface.h"

#define JNI_EXPORT(name) \
    JNIEXPORT jlong JNICALL Java_dev_spektre_surface_SpektreSurface_##name

JNIEXPORT jstring JNICALL
Java_dev_spektre_surface_SpektreSurface_nativeVersion(JNIEnv *env, jclass cls) {
    (void)cls;
    return (*env)->NewStringUTF(env, cos_v76_version);
}

JNIEXPORT jboolean JNICALL
Java_dev_spektre_surface_SpektreSurface_nativeTouchOk(
        JNIEnv *env, jclass cls,
        jint xQ15, jint yQ15, jint pressureQ15, jint phase, jlong timestampMs) {
    (void)env; (void)cls;
    cos_v76_touch_t t = {
        .x_q15        = (uint16_t)xQ15,
        .y_q15        = (uint16_t)yQ15,
        .pressure_q15 = (uint16_t)pressureQ15,
        .phase        = (uint16_t)phase,
        .timestamp_ms = (uint32_t)timestampMs,
    };
    return cos_v76_touch_ok(&t) == 1u ? JNI_TRUE : JNI_FALSE;
}

/* Touch decode: returns a boxed long[4] holding the 256-bit HV. */
JNIEXPORT jlongArray JNICALL
Java_dev_spektre_surface_SpektreSurface_nativeTouchDecode(
        JNIEnv *env, jclass cls,
        jint xQ15, jint yQ15, jint pressureQ15, jint phase, jlong timestampMs) {
    (void)cls;
    cos_v76_touch_t t = {
        .x_q15        = (uint16_t)xQ15,
        .y_q15        = (uint16_t)yQ15,
        .pressure_q15 = (uint16_t)pressureQ15,
        .phase        = (uint16_t)phase,
        .timestamp_ms = (uint32_t)timestampMs,
    };
    cos_v76_hv_t hv; cos_v76_touch_decode(&hv, &t);
    jlong words[4] = {
        (jlong)hv.w[0], (jlong)hv.w[1], (jlong)hv.w[2], (jlong)hv.w[3]
    };
    jlongArray arr = (*env)->NewLongArray(env, 4);
    if (arr != NULL) (*env)->SetLongArrayRegion(env, arr, 0, 4, words);
    return arr;
}

/* 16-bit composed decision.
 * bits: 16-element byte[] with each element 0 or 1, in the order
 *   v60, v61, v62, v63, v64, v65, v66, v67, v68, v69, v70, v71, v72,
 *   v73, v74, v76.
 * Returns 1 iff every kernel allows. */
JNIEXPORT jboolean JNICALL
Java_dev_spektre_surface_SpektreSurface_nativeDecide(
        JNIEnv *env, jclass cls, jbyteArray bits) {
    (void)cls;
    if (bits == NULL) return JNI_FALSE;
    jsize n = (*env)->GetArrayLength(env, bits);
    if (n != 16) return JNI_FALSE;
    jbyte b[16] = {0};
    (*env)->GetByteArrayRegion(env, bits, 0, 16, b);
    cos_v76_decision_t d = cos_v76_compose_decision(
        (uint8_t)b[0],  (uint8_t)b[1],  (uint8_t)b[2],  (uint8_t)b[3],
        (uint8_t)b[4],  (uint8_t)b[5],  (uint8_t)b[6],  (uint8_t)b[7],
        (uint8_t)b[8],  (uint8_t)b[9],  (uint8_t)b[10], (uint8_t)b[11],
        (uint8_t)b[12], (uint8_t)b[13], (uint8_t)b[14], (uint8_t)b[15]);
    return d.allow == 1 ? JNI_TRUE : JNI_FALSE;
}
