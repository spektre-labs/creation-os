/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ≥6 tests for multimodal perception (no live servers required). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "perception.h"

int main(void)
{
    cos_perception_self_test();

    if (cos_perception_init() != 0)
    {
        fputs("check-perception: init failed\n", stderr);
        return 1;
    }

    struct cos_perception_input in;
    struct cos_perception_result res;

    memset(&in, 0, sizeof in);
    memset(&res, 0, sizeof res);
    in.modality = COS_PERCEPTION_IMAGE;
    snprintf(in.filepath, sizeof in.filepath, "%s",
             "/nonexistent/perception_image_probe.jpg");
    if (cos_perception_process(&in, &res) != -1)
    {
        fputs("check-perception: unreadable image should fail\n", stderr);
        return 1;
    }

    memset(&in, 0, sizeof in);
    memset(&res, 0, sizeof res);
    in.modality = COS_PERCEPTION_IMAGE;
    if (cos_perception_process(&in, &res) != -1)
    {
        fputs("check-perception: empty image path should fail\n", stderr);
        return 1;
    }

    memset(&in, 0, sizeof in);
    memset(&res, 0, sizeof res);
    in.modality = COS_PERCEPTION_AUDIO;
    snprintf(in.filepath, sizeof in.filepath, "%s", "/no/such/audio.wav");
    if (cos_perception_process(&in, &res) != -1)
    {
        fputs("check-perception: bad audio path should fail\n", stderr);
        return 1;
    }

    struct cos_perception_result r;
    memset(&r, 0, sizeof r);
    r.sigma = 0.5f;
    if (cos_perception_confidence(&r) < 0.49f
        || cos_perception_confidence(&r) > 0.51f)
    {
        fputs("check-perception: confidence mapping failed\n", stderr);
        return 1;
    }

    return 0;
}
