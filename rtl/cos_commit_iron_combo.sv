// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_commit_iron_combo.sv — irreversible commit allowed only in EXECUTE lane
//
// C–I–L: context and ignored must be non-zero; optional falsifier requires context[0].
// model_lane forbids owning commit (delegation boundary).

`timescale 1ns / 1ps

module cos_commit_iron_combo (
    input  wire [1:0] ctrl,
    input  wire       owner_ok,
    input  wire [7:0] context_tag,
    input  wire [7:0] ignored_mask,
    input  wire       require_falsifier,
    input  wire       model_lane,
    output wire       commit_allowed
);
  wire cil    = (|context_tag) & (|ignored_mask);
  wire fal_ok = !require_falsifier | context_tag[0];
  wire exec   = (ctrl == 2'h0);
  assign commit_allowed = exec & owner_ok & cil & fal_ok & !model_lane;
endmodule
