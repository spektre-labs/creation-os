/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
 * All rights reserved. See LICENSE for binding terms.
 *
 * σ-gate L1-style entropy probe — ESP32 / Arduino sketch (lab).
 * Self-contained C core + Serial harness. Does **not** modify `python/cos/sigma_gate.h`.
 * NOT AGI ACHIEVED — see docs/CLAIM_DISCIPLINE.md.
 */

#include <Arduino.h>
#include <math.h>

typedef struct {
  float threshold_accept;
  float threshold_abstain;
} SigmaConfig;

typedef struct {
  float sigma;
  int verdict; /* 0=ACCEPT, 1=RETHINK, 2=ABSTAIN */
} SigmaResult;

static float token_entropy(const char* text, int len)
{
  int freq[256];
  float entropy;
  int i;
  if (!text || len <= 0) {
    return 0.0f;
  }
  for (i = 0; i < 256; i++) {
    freq[i] = 0;
  }
  for (i = 0; i < len; i++) {
    freq[(unsigned char)text[i]]++;
  }
  entropy = 0.0f;
  for (i = 0; i < 256; i++) {
    if (freq[i] > 0) {
      float p = (float)freq[i] / (float)len;
      entropy -= p * log2f(p);
    }
  }
  return entropy;
}

static SigmaResult sigma_score(
    const char* prompt,
    int plen,
    const char* response,
    int rlen,
    const SigmaConfig* config)
{
  SigmaResult result;
  float ent;
  float max_ent;
  (void)prompt;
  (void)plen;

  if (rlen == 0 || !response || !config) {
    result.sigma = 1.0f;
    result.verdict = 2;
    return result;
  }

  ent = token_entropy(response, rlen);
  max_ent = log2f(256.0f);
  result.sigma = 1.0f - (ent / max_ent);

  if (result.sigma < 0.0f) {
    result.sigma = 0.0f;
  }
  if (result.sigma > 1.0f) {
    result.sigma = 1.0f;
  }

  if (result.sigma < config->threshold_accept) {
    result.verdict = 0;
  } else if (result.sigma > config->threshold_abstain) {
    result.verdict = 2;
  } else {
    result.verdict = 1;
  }

  return result;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("sigma-gate ESP32 ready");
}

void loop()
{
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    SigmaConfig config;
    SigmaResult result;
    config.threshold_accept = 0.15f;
    config.threshold_abstain = 0.85f;
    result = sigma_score(
        "prompt",
        6,
        input.c_str(),
        (int)input.length(),
        &config);
    Serial.printf(
        "sigma=%.4f verdict=%s\n",
        result.sigma,
        result.verdict == 0 ? "ACCEPT" : (result.verdict == 1 ? "RETHINK" : "ABSTAIN"));
  }
  delay(10);
}
