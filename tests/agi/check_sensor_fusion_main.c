/* ≥4 tests for σ-sensor fusion. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sensor_fusion.h"

int main(void)
{
    cos_sensor_fusion_self_test();

    struct cos_fusion_result fused;
    struct cos_perception_result r;
    memset(&r, 0, sizeof r);
    snprintf(r.description, sizeof r.description, "%s", "only one");
    r.sigma    = 0.4f;
    r.modality = COS_PERCEPTION_TEXT;
    if (cos_sensor_fusion(&r, 1, &fused) != 0
        || fabsf(fused.sigma_fused - 0.4f) > 0.01f
        || fused.dominant_modality != COS_PERCEPTION_TEXT
        || fused.n_modalities != 1)
    {
        fputs("check-sensor-fusion: single-input fusion failed\n", stderr);
        return 1;
    }

    struct cos_perception_result t, i, a;
    memset(&t, 0, sizeof t);
    memset(&i, 0, sizeof i);
    memset(&a, 0, sizeof a);
    snprintf(t.description, sizeof t.description, "t");
    snprintf(i.description, sizeof i.description, "i");
    snprintf(a.description, sizeof a.description, "a");
    t.sigma = 0.1f;
    i.sigma = 0.2f;
    a.sigma = 0.3f;
    t.modality   = COS_PERCEPTION_TEXT;
    i.modality   = COS_PERCEPTION_IMAGE;
    a.modality   = COS_PERCEPTION_AUDIO;
    struct cos_perception_result three[3] = { t, i, a };
    if (cos_sensor_fusion(three, 3, &fused) != 0)
    {
        fputs("check-sensor-fusion: three-way failed\n", stderr);
        return 1;
    }
    if (fabsf(fused.sigma_per_modality[0] - 0.1f) > 0.02f
        || fabsf(fused.sigma_per_modality[1] - 0.2f) > 0.02f
        || fabsf(fused.sigma_per_modality[2] - 0.3f) > 0.02f)
    {
        fputs("check-sensor-fusion: per-modality max failed\n", stderr);
        return 1;
    }

    return 0;
}
