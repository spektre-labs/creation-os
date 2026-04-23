/*
 * cos-sense · multimodal perception CLI (see / hear / sense).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <time.h>
#include <unistd.h>

#include "../sigma/perception.h"
#include "../sigma/sensor_fusion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_result(const char *label, const struct cos_perception_result *r)
{
    printf("%s\n  description: %s\n  sigma: %.4f  confidence: %.4f  modality: %d  "
           "latency_ms: %lld  attribution: %d\n",
           label, r->description, (double)r->sigma,
           (double)cos_perception_confidence(r), r->modality,
           (long long)r->latency_ms, (int)r->attribution);
}

static void print_fusion(const struct cos_fusion_result *f)
{
    printf("fused\n  sigma_fused: %.4f  modalities: %d  dominant_modality: %d  "
           "contradiction: %d\n",
           (double)f->sigma_fused, f->n_modalities, f->dominant_modality,
           f->contradiction);
    printf("  sigma_per_modality [text,image,audio]: %.4f %.4f %.4f\n",
           (double)f->sigma_per_modality[0], (double)f->sigma_per_modality[1],
           (double)f->sigma_per_modality[2]);
    printf("  text: %s\n", f->fused_description);
}

static int parse_common(struct cos_perception_input *in, int argc, char **argv,
                        int need_image, int need_audio)
{
    memset(in, 0, sizeof *in);
    in->timestamp = (int64_t)time(NULL) * 1000;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
            snprintf(in->filepath, sizeof in->filepath, "%s", argv[++i]);
        } else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
            snprintf(in->filepath, sizeof in->filepath, "%s", argv[++i]);
        } else if (argv[i][0] != '-') {
            /* remainder: prompt text */
            size_t off = strlen(in->text);
            if (off + 2 < sizeof in->text) {
                if (off > 0)
                    in->text[off++] = ' ';
                snprintf(in->text + off, sizeof in->text - off, "%s", argv[i]);
            }
        }
    }
    if (need_image && (!in->filepath[0] || access(in->filepath, R_OK) != 0))
        return -1;
    if (need_audio && (!in->filepath[0] || access(in->filepath, R_OK) != 0))
        return -1;
    return 0;
}

int cos_see_main(int argc, char **argv)
{
    struct cos_perception_input in;
    if (parse_common(&in, argc, argv, 1, 0) != 0) {
        fprintf(stderr,
                "usage: cos see --image PATH [prompt words...]\n");
        return 2;
    }
    in.modality = COS_PERCEPTION_IMAGE;
    struct cos_perception_result res;
    cos_perception_init();
    int rc = cos_perception_process(&in, &res);
    print_result("vision", &res);
    return rc != 0 ? 1 : 0;
}

int cos_hear_main(int argc, char **argv)
{
    struct cos_perception_input in;
    if (parse_common(&in, argc, argv, 0, 1) != 0) {
        fprintf(stderr,
                "usage: cos hear --audio PATH [prompt words...]\n");
        return 2;
    }
    in.modality = COS_PERCEPTION_AUDIO;
    struct cos_perception_result res;
    cos_perception_init();
    int rc = cos_perception_process(&in, &res);
    print_result("audio", &res);
    return rc != 0 ? 1 : 0;
}

int cos_sense_main(int argc, char **argv)
{
    struct cos_perception_input im, au;
    memset(&im, 0, sizeof im);
    memset(&au, 0, sizeof au);
    im.timestamp = au.timestamp = (int64_t)time(NULL) * 1000;

    char img_path[512] = "", aud_path[512] = "";
    char prompt[4096]   = "";

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i + 1 < argc)
            snprintf(img_path, sizeof img_path, "%s", argv[++i]);
        else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc)
            snprintf(aud_path, sizeof aud_path, "%s", argv[++i]);
        else if (argv[i][0] != '-') {
            size_t off = strlen(prompt);
            if (off + 2 < sizeof prompt) {
                if (off > 0)
                    prompt[off++] = ' ';
                snprintf(prompt + off, sizeof prompt - off, "%s", argv[i]);
            }
        }
    }

    int n = 0;
    struct cos_perception_result results[3];
    memset(results, 0, sizeof results);

    if (img_path[0] && access(img_path, R_OK) == 0) {
        snprintf(im.filepath, sizeof im.filepath, "%s", img_path);
        snprintf(im.text, sizeof im.text, "%s", prompt);
        im.modality = COS_PERCEPTION_IMAGE;
        cos_perception_init();
        if (cos_perception_process(&im, &results[n]) == 0)
            n++;
    }
    if (aud_path[0] && access(aud_path, R_OK) == 0) {
        snprintf(au.filepath, sizeof au.filepath, "%s", aud_path);
        snprintf(au.text, sizeof au.text, "%s", prompt);
        au.modality = COS_PERCEPTION_AUDIO;
        cos_perception_init();
        if (cos_perception_process(&au, &results[n]) == 0)
            n++;
    }
    if (prompt[0]) {
        struct cos_perception_input tx;
        memset(&tx, 0, sizeof tx);
        snprintf(tx.text, sizeof tx.text, "%s", prompt);
        tx.modality   = COS_PERCEPTION_TEXT;
        tx.timestamp  = (int64_t)time(NULL) * 1000;
        cos_perception_init();
        if (cos_perception_process(&tx, &results[n]) == 0)
            n++;
    }

    if (n == 0) {
        fprintf(stderr,
                "usage: cos sense --image PATH [--audio PATH] [prompt]\n");
        return 2;
    }

    for (int i = 0; i < n; i++)
        print_result(n == 1 ? "perception" : "modality", &results[i]);

    if (n > 1) {
        struct cos_fusion_result fused;
        memset(&fused, 0, sizeof fused);
        if (cos_sensor_fusion(results, n, &fused) == 0)
            print_fusion(&fused);
    }
    return 0;
}

#ifndef COS_SENSE_STANDALONE_MAIN
/* Linked from cos(1) via cos-sense binary; standalone defines main. */
#else
static void usage_all(void)
{
    fputs(
        "cos-sense — multimodal σ perception\n"
        "  cos-sense see --image FILE [prompt]\n"
        "  cos-sense hear --audio FILE [prompt]\n"
        "  cos-sense sense --image FILE [--audio FILE] [prompt]\n"
        "  COS_VISION_SERVER  COS_VISION_MODEL  COS_AUDIO_SERVER  COS_AUDIO_MODEL\n",
        stderr);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage_all();
        return 2;
    }
    if (strcmp(argv[1], "see") == 0)
        return cos_see_main(argc - 2, argv + 2);
    if (strcmp(argv[1], "hear") == 0)
        return cos_hear_main(argc - 2, argv + 2);
    if (strcmp(argv[1], "sense") == 0)
        return cos_sense_main(argc - 2, argv + 2);
    usage_all();
    return 2;
}
#endif
