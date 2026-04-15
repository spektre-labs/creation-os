// SPDX-License-Identifier: AGPL-3.0-or-later
package spektre.core

import chisel3._
import chisel3.util.PopCount

/** Control triad — parity with rtl/cos_agency_iron_combo.sv */
class AgencyIronCombo extends Module {
  val io = IO(new Bundle {
    val trapMask = Input(UInt(7.W))
    val stateDeclared = Input(Bool())
    val anchorOk = Input(Bool())
    val eventStateAmp = Input(Bool())
    val urgencyOnly = Input(Bool())
    val ctrl = Output(UInt(2.W))
  })

  val pc = PopCount(io.trapMask)
  val anchorOv = io.trapMask(6)
  val stab = io.eventStateAmp || (pc >= 2.U) || anchorOv
  val anyTrap = io.trapMask.orR
  val naked = !io.stateDeclared || !io.anchorOk
  val hold = anyTrap || naked || io.urgencyOnly
  io.ctrl := Mux(stab, 2.U(2.W), Mux(hold, 1.U(2.W), 0.U(2.W)))
}
