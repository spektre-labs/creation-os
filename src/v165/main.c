/*
 * v165 σ-Edge — CLI entry point.
 *
 *   creation_os_v165_edge                   # profile table JSON
 *   creation_os_v165_edge --self-test
 *   creation_os_v165_edge --boot TARGET     # boot receipt JSON
 *   creation_os_v165_edge --tau TAU_DEFAULT AVAIL_MB
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "edge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cos_v165_target_t parse_target(const char *name) {
    if (strcmp(name, "macbook_m3")   == 0) return COS_V165_TARGET_MACBOOK_M3;
    if (strcmp(name, "rpi5")         == 0) return COS_V165_TARGET_RPI5;
    if (strcmp(name, "jetson_orin")  == 0) return COS_V165_TARGET_JETSON_ORIN;
    if (strcmp(name, "jetson")       == 0) return COS_V165_TARGET_JETSON_ORIN;
    if (strcmp(name, "android")      == 0) return COS_V165_TARGET_ANDROID;
    if (strcmp(name, "ios")          == 0) return COS_V165_TARGET_IOS;
    return (cos_v165_target_t)-1;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v165_self_test();
        if (rc == 0) puts("v165 self-test: ok");
        else         fprintf(stderr, "v165 self-test failed at stage %d\n", rc);
        return rc;
    }

    if (argc >= 3 && strcmp(argv[1], "--boot") == 0) {
        int t = (int)parse_target(argv[2]);
        if (t < 0) { fprintf(stderr, "unknown target: %s\n", argv[2]); return 1; }
        float tau_default = argc >= 4 ? (float)atof(argv[3]) : 0.5f;
        cos_v165_boot_receipt_t r = cos_v165_boot_cos_lite(
            (cos_v165_target_t)t, tau_default);
        char buf[1024];
        size_t n = cos_v165_boot_to_json(&r, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v165: json overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        putchar('\n');
        return r.booted ? 0 : 1;
    }

    if (argc >= 4 && strcmp(argv[1], "--tau") == 0) {
        float tau_default = (float)atof(argv[2]);
        int avail_mb = atoi(argv[3]);
        float tau_edge = cos_v165_tau_edge(tau_default, avail_mb);
        printf("{\"kernel\":\"v165\",\"event\":\"tau\","
               "\"tau_default\":%.4f,\"available_ram_mb\":%d,"
               "\"tau_edge\":%.4f}\n",
               (double)tau_default, avail_mb, (double)tau_edge);
        return 0;
    }

    float tau_default = 0.5f;
    if (argc >= 3 && strcmp(argv[1], "--list") == 0) {
        tau_default = (float)atof(argv[2]);
    }
    char buf[4096];
    size_t n = cos_v165_profiles_to_json(buf, sizeof(buf), tau_default, NULL);
    if (n == 0) { fprintf(stderr, "v165: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout);
    putchar('\n');
    return 0;
}
