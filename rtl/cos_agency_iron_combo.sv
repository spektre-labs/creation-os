// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_agency_iron_combo.sv — control triad (EXECUTE / HOLD / STABILIZE)
//
// Mirrors portable C policy: amplification, ≥2 traps, or anchor-override → STABILIZE;
// any trap / missing state or anchor / urgency-only → HOLD; else EXECUTE.
// trap_mask[6] = ANCHOR_OVR per cos_agency_iron.h

module cos_agency_iron_combo (
    input  wire [6:0] trap_mask,
    input  wire       state_declared,
    input  wire       anchor_ok,
    input  wire       event_state_amp,
    input  wire       urgency_only,
    output wire [1:0] ctrl
);
  wire [2:0] pc = 3'd($countones(trap_mask));
  wire       anchor_ov = trap_mask[6];
  wire       stab      = event_state_amp | (pc >= 3'd2) | anchor_ov;
  wire       anytrap   = |trap_mask;
  wire       naked     = !state_declared | !anchor_ok;
  wire       hold      = anytrap | naked | urgency_only;

  assign ctrl = stab ? 2'h2 : (hold ? 2'h1 : 2'h0);

endmodule
