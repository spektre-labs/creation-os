# CREATION OS — Σ-Native Logic (SNL v1.1). Kernel-level symbol → ISA-shaped ops. 1 = 1
from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple, Union

import numpy as np

from creation_os_core import query_hash_key
from sk9_bindings import get_sk9

SNL_VERSION = "1.1"

# Symbol → hardware-shaped meaning (documentation contract; NEON lives in libsk9).
SNL_SYMBOL_ISA: Dict[str, str] = {
    "#": "GDA_HASH / BBHash address (identity of M*)",
    "!": "POPCNT — σ audit on register bits",
    "^": "XOR — observation ⊗ model difference",
    "&": "BITMASK AND — The Cut / coherence filter",
    "@": "PREFETCH / LDR — attention load target",
    "=": "INVARIANT — CMP fixed point (1=1)",
    "*": "FMA-shaped weight — steer vector toward anchor",
    "?": "Epistemic uncertainty — σ_norm∈(0.1,0.9) ⇒ surface appendix arm",
}

UNCERTAINTY_APPENDIX = (
    "1=1. Epistemic uncertainty: this claim requires external verification or an update."
)
UNCERTAINTY_APPENDIX_FI = UNCERTAINTY_APPENDIX  # backward-compatible alias


def _snl_at_omega_warm(tail: str) -> None:
    try:
        import source_vacuum as _sv

        _sv.source_vacuum_absorb()
    except Exception:
        pass
    if os.environ.get("CREATION_OS_SNL_AT_OMEGA", "").strip() not in ("1", "true", "yes"):
        return
    if not tail.strip():
        return
    try:
        import source_vacuum as _sv2

        _sv2.source_vacuum_on_attention_tail(tail)
    except Exception:
        pass


def tokenize_snl(source: str) -> List[Tuple[str, str]]:
    """
    Tokenize compact SNL: operators are single chars in # ! ^ & @ = * ;
    everything else becomes consecutive 'id' tokens (alphanumeric + underscore).
    """
    s = re.sub(r"//.*", "", source)
    i, n = 0, len(s)
    ops = set(SNL_SYMBOL_ISA.keys())
    out: List[Tuple[str, str]] = []
    while i < n:
        c = s[i]
        if c.isspace():
            i += 1
            continue
        if c in ops:
            out.append(("op", c))
            i += 1
            continue
        j = i
        while j < n and (not s[j].isspace()) and s[j] not in ops:
            j += 1
        if j > i:
            out.append(("id", s[i:j]))
            i = j
        else:
            i += 1
    return out


def _u32(x: Any) -> np.uint32:
    return np.uint32(int(x) & 0xFFFFFFFF)


@dataclass
class SNLState:
    """Z_n registers: uint32 lanes by default; Q is float32[4] for invariant hooks."""

    regs: Dict[str, np.uint32] = field(default_factory=dict)
    Q: np.ndarray = field(default_factory=lambda: np.zeros(4, dtype=np.float32))
    sigma: int = 0
    last_key: int = 0
    last_route: int = -1
    last_bb_val: Optional[int] = None
    prefetch: str = ""
    attn_arm: bool = False
    invariant_ok: bool = False
    uncertainty_append_required: bool = False
    log: List[str] = field(default_factory=list)

    def zget(self, name: str) -> np.uint32:
        return self.regs.get(name.upper(), np.uint32(0))

    def zset(self, name: str, v: Union[int, np.uint32]) -> None:
        self.regs[name.upper()] = _u32(v)


class SNLRunner:
    """
    Execute SNL v1.0 token streams against sk9 (optional) and numeric Z-registers.
    """

    def __init__(self, sk9: Any = None) -> None:
        self._sk9 = sk9 if sk9 is not None else get_sk9()

    def run_tokens(self, tokens: List[Tuple[str, str]], state: SNLState) -> SNLState:
        sk9 = self._sk9
        i = 0
        while i < len(tokens):
            kind, val = tokens[i]
            if kind != "op":
                state.log.append(f"skip dangling id {val!r}")
                i += 1
                continue
            op = val
            i += 1

            if op == "@":
                if i < len(tokens) and tokens[i][0] == "id":
                    state.prefetch = tokens[i][1]
                    state.log.append(f"@ prefetch {state.prefetch!r}")
                    _snl_at_omega_warm(state.prefetch)
                    i += 1
                else:
                    state.attn_arm = True
                    state.log.append("@ attention arm → binds to next # key (Metal-identity load)")

            elif op == "#":
                key_s = ""
                if i < len(tokens) and tokens[i][0] == "id":
                    key_s = tokens[i][1]
                    i += 1
                if state.attn_arm:
                    state.prefetch = key_s
                    state.attn_arm = False
                    state.log.append(f"@# M* bind: prefetch ← {key_s!r}")
                    _snl_at_omega_warm(state.prefetch)
                state.last_key = query_hash_key(key_s) if key_s else 0
                state.log.append(f"# GDA key {state.last_key:#018x} ({key_s!r})")
                if sk9 is not None and getattr(sk9, "ok", False):
                    route, v = sk9.epistemic_route(state.last_key)
                    state.last_route = route
                    state.last_bb_val = v
                    state.log.append(f"# route={route} bb_val={v}")

            elif op == "!":
                reg = "Z0"
                if i < len(tokens) and tokens[i][0] == "id":
                    reg = tokens[i][1]
                    i += 1
                v = int(state.zget(reg)) & 0xFFFFFFFF
                if os.environ.get("CREATION_OS_SNL_FAST_PATH", "").strip() in ("1", "true", "yes"):
                    try:
                        from snl_fast_path import fast_popcnt32

                        state.sigma = int(fast_popcnt32(v))
                    except Exception:
                        state.sigma = (
                            int(v.bit_count()) if hasattr(int, "bit_count") else bin(v).count("1")
                        )
                else:
                    state.sigma = int(v.bit_count()) if hasattr(int, "bit_count") else bin(v).count("1")
                state.log.append(f"! σ={state.sigma} on {reg}={v:#010x}")

            elif op == "^":
                a = b = "Z0"
                if i < len(tokens) and tokens[i][0] == "id":
                    a = tokens[i][1]
                    i += 1
                if i < len(tokens) and tokens[i][0] == "id":
                    b = tokens[i][1]
                    i += 1
                x = int(state.zget(a)) ^ int(state.zget(b))
                state.zset(a, x)
                state.log.append(f"^{a} ← {a}⊕{b} = {x:#010x}")

            elif op == "&":
                reg = "Z0"
                mask = 0xFFFFFFFF
                if i < len(tokens) and tokens[i][0] == "id":
                    reg = tokens[i][1]
                    i += 1
                if i < len(tokens) and tokens[i][0] == "id":
                    mask = int(tokens[i][1], 0)
                    i += 1
                y = int(state.zget(reg)) & mask
                state.zset(reg, y)
                state.log.append(f"&{reg} ← {y:#010x}")

            elif op == "*":
                reg = "Z0"
                scale, bias = 1.0, 0.0
                if i < len(tokens) and tokens[i][0] == "id":
                    reg = tokens[i][1]
                    i += 1
                if i < len(tokens) and tokens[i][0] == "id":
                    scale = float(tokens[i][1])
                    i += 1
                if i < len(tokens) and tokens[i][0] == "id":
                    bias = float(tokens[i][1])
                    i += 1
                q = state.Q.astype(np.float64, copy=False)
                q *= scale
                q += bias
                state.Q = q.astype(np.float32)
                state.log.append(f"*{reg} FMA-shaped Q scale={scale} bias={bias}")

            elif op == "=":
                ok = state.sigma == 0
                if (
                    sk9 is not None
                    and getattr(sk9, "genesis_ok", False)
                    and float(np.max(np.abs(state.Q))) > 1e-6
                ):
                    try:
                        ok = ok and bool(sk9.genesis_verify(state.Q))
                    except Exception:
                        ok = ok and False
                state.invariant_ok = bool(ok)
                state.log.append(f"= invariant_ok={state.invariant_ok} (σ==0 hard gate)")

            elif op == "?":
                sig = int(state.sigma)
                sn = min(1.0, float(sig) / 8.0) if sig > 0 else 0.0
                if 0.1 < sn < 0.9:
                    state.uncertainty_append_required = True
                    state.log.append(
                        f"? σ_norm={sn:.3f} → append uncertainty appendix: {UNCERTAINTY_APPENDIX!r}"
                    )
                else:
                    state.uncertainty_append_required = False
                    state.log.append(f"? σ_norm={sn:.3f} → no appendix band")

            else:
                state.log.append(f"unknown op {op!r}")

        return state

    def run_source(self, source: str, state: Optional[SNLState] = None) -> SNLState:
        st = state or SNLState()
        return self.run_tokens(tokenize_snl(source), st)


def snl_demo_pipeline() -> str:
    """Canonical v1.0 micro-pipeline (compact + commented)."""
    return """
    @ M
    # manifesto
    ! Z0
    ^ Z0 Z1
    & Z0 0xffffffff
    =
    """


if __name__ == "__main__":
    prog = "@#M* // attention → identity anchor → FMA steer"
    st = SNLState()
    st.zset("Z1", 0x00FF00FF)
    r = SNLRunner()
    r.run_source(prog, st)
    print("SNL", SNL_VERSION, "tokens:", tokenize_snl(prog))
    for line in st.log:
        print(line)
    print("invariant_ok:", st.invariant_ok, "sigma:", st.sigma)
