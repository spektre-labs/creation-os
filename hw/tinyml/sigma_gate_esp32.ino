// SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
// σ-gate ESP32 lab: serial Q16 σ in → LED verdict out (ACCEPT / RETHINK / ABSTAIN).
//
// Pedagogy: treat the RGB bank as a tiny “σ meter” (one visible pixel of epistemic state).
//
// Open this directory as an Arduino/PlatformIO sketch; keep sigma_gate_tiny.h alongside.

#include "sigma_gate_tiny.h"

#define LED_GREEN  2
#define LED_YELLOW 4
#define LED_RED    5

static sigma_state_t gate;

static void show_verdict(sigma_verdict_t v) {
    digitalWrite(LED_GREEN, v == SIGMA_ACCEPT ? HIGH : LOW);
    digitalWrite(LED_YELLOW, v == SIGMA_RETHINK ? HIGH : LOW);
    digitalWrite(LED_RED, v == SIGMA_ABSTAIN ? HIGH : LOW);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    sigma_state_init(&gate);
    Serial.println("sigma-gate ESP32 ready (send Q16 sigma as integer, e.g. 16384 ~= 0.25)");
}

void loop() {
    if (Serial.available()) {
        long v = Serial.parseInt();
        int32_t sigma_q16 = (int32_t)v;
        sigma_update(&gate, sigma_q16, SIGMA_Q16(0.9));
        sigma_verdict_t verdict = sigma_gate(&gate);
        show_verdict(verdict);

        Serial.print("sigma_q16=");
        Serial.print((long)gate.sigma);
        Serial.print(" k_eff=");
        Serial.print((long)gate.k_eff);
        Serial.print(" verdict=");
        if (verdict == SIGMA_ACCEPT)
            Serial.println("ACCEPT");
        else if (verdict == SIGMA_RETHINK)
            Serial.println("RETHINK");
        else
            Serial.println("ABSTAIN");
    }
    delay(10);
}
