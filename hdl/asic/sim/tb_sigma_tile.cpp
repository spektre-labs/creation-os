// SPDX-License-Identifier: AGPL-3.0-or-later
// Verilator smoke for tt_um_sigma_tile (SPI-ish shift interface).

#include "Vtt_um_sigma_tile.h"
#include "verilated.h"
#include <cstdio>

static void tick(Vtt_um_sigma_tile *top) {
  top->clk = 0;
  top->eval();
  top->clk = 1;
  top->eval();
}

static void pulse_sck(Vtt_um_sigma_tile *top, int mosi) {
  top->ui_in = static_cast<uint8_t>((mosi ? 1 : 0) | (0 << 1));
  tick(top);
  top->ui_in = static_cast<uint8_t>((mosi ? 1 : 0) | (1 << 1));
  tick(top);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Verilated::commandArgs(argc, argv);
  Vtt_um_sigma_tile *top = new Vtt_um_sigma_tile;

  const int HV = 128; // must match Verilator -G HV_BITS=128

  top->rst_n = 0;
  top->ena = 1;
  top->uio_in = 0;
  tick(top);
  top->rst_n = 1;
  tick(top);

  for (int i = 0; i < HV; i++) {
    pulse_sck(top, 1);
  }
  for (int i = 0; i < HV; i++) {
    pulse_sck(top, 1);
  }

  top->ui_in = 0;
  for (int i = 0; i < 6; i++) {
    tick(top);
  }

  int bits[14];
  bits[0] = static_cast<int>(top->uo_out & 1);
  for (int i = 1; i < 14; i++) {
    pulse_sck(top, 0);
    bits[i] = static_cast<int>(top->uo_out & 1);
  }

  const int abstain = static_cast<int>((top->uo_out >> 7) & 1);
  printf("abstain=%d pop_miso_first=%d\n", abstain, bits[0]);

  if (abstain != 0) {
    fprintf(stderr, "unexpected abstain (expected 0 for identical Q/K)\n");
    delete top;
    return 2;
  }

  delete top;
  return 0;
}
