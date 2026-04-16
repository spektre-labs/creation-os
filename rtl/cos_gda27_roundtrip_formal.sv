// SPDX-License-Identifier: AGPL-3.0-or-later
// Combinational BMC: base-27 digit alphabet matches C (`gda27_stub.c`) for 0..26.
`default_nettype none
module cos_gda27_roundtrip_formal;
  function automatic reg [7:0] digit_char(input integer d);
    begin
      digit_char = 8'h30;
      case (d)
        0: digit_char = 8'h30;  /* '0' */
        1: digit_char = 8'h31;
        2: digit_char = 8'h32;
        3: digit_char = 8'h33;
        4: digit_char = 8'h34;
        5: digit_char = 8'h35;
        6: digit_char = 8'h36;
        7: digit_char = 8'h37;
        8: digit_char = 8'h38;
        9: digit_char = 8'h39;
        10: digit_char = 8'h61; /* 'a' */
        11: digit_char = 8'h62;
        12: digit_char = 8'h63;
        13: digit_char = 8'h64;
        14: digit_char = 8'h65;
        15: digit_char = 8'h66;
        16: digit_char = 8'h67;
        17: digit_char = 8'h68;
        18: digit_char = 8'h69;
        19: digit_char = 8'h6a;
        20: digit_char = 8'h6b;
        21: digit_char = 8'h6c;
        22: digit_char = 8'h6d;
        23: digit_char = 8'h6e;
        24: digit_char = 8'h6f;
        25: digit_char = 8'h70;
        26: digit_char = 8'h71; /* 'q' */
        default: digit_char = 8'h3f;
      endcase
    end
  endfunction

  function automatic int chr_to_idx(input reg [7:0] c);
    begin
      if (c >= 8'h30 && c <= 8'h39)
        chr_to_idx = int'(c - 8'h30);
      else if (c >= 8'h61 && c <= 8'h71)
        chr_to_idx = 10 + int'(c - 8'h61);
      else
        chr_to_idx = -1;
    end
  endfunction

  genvar vi;
  generate
    for (vi = 0; vi < 27; vi = vi + 1) begin : G
      wire [7:0] ch;
      assign ch = digit_char(vi);
      always_comb begin
        assert(chr_to_idx(ch) == vi);
      end
    end
  endgenerate
endmodule

`default_nettype wire
