/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * ATtiny85 lab template: σ-gate + three pins for verdict LEDs (if routed).
 * Build with avr-gcc -mmcu=attiny85 — not part of the host merge-gate.
 *
 * PB0 = ACCEPT, PB1 = RETHINK, PB2 = ABSTAIN (active high; add resistors + LEDs).
 */
#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include "sigma_gate_tiny.h"
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

static void pins_init(void)
{
    DDRB |= (uint8_t)((1 << PB0) | (1 << PB1) | (1 << PB2));
}

static void show_verdict(sigma_verdict_t v)
{
    uint8_t mask = 0;
    if (v == SIGMA_ACCEPT)
        mask |= (1 << PB0);
    if (v == SIGMA_RETHINK)
        mask |= (1 << PB1);
    if (v == SIGMA_ABSTAIN)
        mask |= (1 << PB2);
    PORTB = (PORTB & (uint8_t)~((1 << PB0) | (1 << PB1) | (1 << PB2))) | mask;
}

int main(void)
{
    sigma_state_t gate;

    pins_init();
    sigma_state_init(&gate);

    for (;;) {
        /* Demo: ramp σ in Q16 — replace with ADC / UART in a real build. */
        int32_t sigma_q16 = 12000;
        sigma_update(&gate, sigma_q16, SIGMA_Q16(0.9));
        show_verdict(sigma_gate(&gate));
        _delay_ms(200);
    }
    return 0;
}
