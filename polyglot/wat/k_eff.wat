;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; k_eff.wat — K_eff = (K * (256 - sigma_q8)) >> 8  (same as rtl/cos_k_eff_bind.sv)
;;; wat2wasm k_eff.wat -o k_eff.wasm
;;; wasmtime run k_eff.wasm --invoke k_eff 4000:i32 128:i32  → 2000
(module
  (func $k_eff (param $k i32) (param $s i32) (result i32)
    (i32.shr_u
      (i32.mul (local.get $k) (i32.sub (i32.const 256) (local.get $s)))
      (i32.const 8)))
  (export "k_eff" (func $k_eff))
)
