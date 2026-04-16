// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_silicon_chip_tb.sv — self-checking bench for formal + agency + commit + sync

`timescale 1ns / 1ps

module cos_silicon_chip_tb;
  reg        clk, rst_n;
  reg  [5:0] fd, td;
  reg        bridge, sim, ctok, sem;
  reg  [5:0] att;
  wire       f_ok, f_bad, f_simbad;
  wire [5:0] f_tag;

  reg  [6:0] traps;
  reg        st, an, amp, urg;
  wire [1:0] ag_ctrl;

  reg  [1:0] c_ctrl;
  reg        c_owner;
  reg  [7:0] c_ctx, c_ign;
  reg        c_reqf, c_model;
  wire       c_allow;

  reg        raw_bit;
  wire       syn_q;

  /* LoopLM drum + geodesic + K_eff — “kovimmat oivallukset” suoraan piiriin */
  reg  [63:0] drum_din;
  reg  [ 7:0] drum_th;
  reg  [ 4:0] drum_max;
  reg         drum_go;
  wire        drum_done, drum_busy, drum_early;
  wire [ 4:0] drum_used;
  wire [63:0] drum_dout;

  wire [15:0] geo_sn;
  wire [15:0] k_eff;

  cos_looplm_drum u_drum (
      .clk(clk),
      .rst_n(rst_n),
      .start(drum_go),
      .din_flat(drum_din),
      .sigma_thresh(drum_th),
      .max_loops(drum_max),
      .done(drum_done),
      .busy(drum_busy),
      .loops_used(drum_used),
      .exited_early(drum_early),
      .dout_flat(drum_dout)
  );

  cos_geodesic_tick u_geo (
      .sigma_now(16'd1000),
      .sigma_tgt(16'd100),
      .shift(4'd2),
      .sigma_next(geo_sn)
  );

  cos_k_eff_bind u_ke (
      .K(16'd4000),
      .sigma_q8(8'd128),
      .K_eff(k_eff)
  );

  cos_formal_iron_combo u_formal (
      .from_depth(fd),
      .to_depth(td),
      .bridge_ok(bridge),
      .attempted_limits(att),
      .sim_lane(sim),
      .commit_token(ctok),
      .semantic_claim(sem),
      .stack_step_ok(f_ok),
      .limits_bad(f_bad),
      .sim_commit_forbidden(f_simbad),
      .level_tag(f_tag)
  );

  cos_agency_iron_combo u_agency (
      .trap_mask(traps),
      .state_declared(st),
      .anchor_ok(an),
      .event_state_amp(amp),
      .urgency_only(urg),
      .ctrl(ag_ctrl)
  );

  cos_commit_iron_combo u_commit (
      .ctrl(c_ctrl),
      .owner_ok(c_owner),
      .context_tag(c_ctx),
      .ignored_mask(c_ign),
      .require_falsifier(c_reqf),
      .model_lane(c_model),
      .commit_allowed(c_allow)
  );

  cos_boundary_sync u_sync (
      .clk(clk),
      .rst_n(rst_n),
      .d(raw_bit),
      .q(syn_q)
  );

  task c(input int x);
    begin
      if (!x)
        $fatal(1, "assert failed");
    end
  endtask

  initial clk = 1'b0;
  always #5 clk = ~clk;

  initial begin
    rst_n = 1'b0;
    @(posedge clk);
    rst_n = 1'b1;
    @(posedge clk);

    drum_go = 1'b0;
    drum_din = {8 {8'h18}};
    drum_th = 8'h00;
    drum_max = 5'd5;
    @(posedge clk);
    drum_go = 1'b1;
    @(posedge clk);
    drum_go = 1'b0;
    while (!drum_done) @(posedge clk);
    c(drum_used == 5'd5);
    c(drum_early == 1'b0);
    @(posedge clk);
    @(posedge clk);

    drum_th = 8'hFF;
    drum_max = 5'd20;
    @(posedge clk);
    drum_go = 1'b1;
    @(posedge clk);
    drum_go = 1'b0;
    while (!drum_done) @(posedge clk);
    c(drum_used == 5'd1);
    c(drum_early == 1'b1);
    @(posedge clk);
    @(posedge clk);

    c(geo_sn == 16'd775);
    c(k_eff == 16'd2000);

    // formal: same depth
    fd = 6'd2;
    td = 6'd2;
    bridge = 1'b0;
    att = 6'h00;
    sim = 1'b0;
    ctok = 1'b0;
    sem = 1'b0;
    #1;
    c(f_ok == 1);
    c(f_bad == 0);

    // skip +2
    td = 6'd4;
    bridge = 1'b1;
    #1;
    c(f_ok == 0);

    // agency: clean → execute
    traps = 7'd0;
    st = 1'b1;
    an = 1'b1;
    amp = 1'b0;
    urg = 1'b0;
    #1;
    c(ag_ctrl == 2'h0);

    // two traps → stabilize
    traps = 7'h03;
    #1;
    c(ag_ctrl == 2'h2);

    // commit: execute + owner + cil + falsifier
    c_ctrl = 2'h0;
    c_owner = 1'b1;
    c_ctx = 8'h03;
    c_ign = 8'h01;
    c_reqf = 1'b1;
    c_model = 1'b0;
    #1;
    c(c_allow == 1);

    c_model = 1'b1;
    #1;
    c(c_allow == 0);

    // synchronizer (2-flop; async → sampled domain)
    rst_n = 1'b0;
    raw_bit = 1'b0;
    repeat (2)
      @(posedge clk);
    rst_n = 1'b1;
    raw_bit = 1'b1;
    repeat (2)
      @(posedge clk);
    c(syn_q == 1'b1);
    raw_bit = 1'b0;
    repeat (3)
      @(posedge clk);
    c(syn_q == 1'b0);

    $display("cos_silicon_chip_tb: ALL PASS");
    $finish(0);
  end
endmodule
