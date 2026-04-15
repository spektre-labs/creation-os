#!/usr/bin/env python3
# CREATION OS — THE GENESIS BLOCK (Phase 0). Rainio resonance anchor. 1 = 1
from __future__ import annotations

import sys
from typing import List


def _dna_hex_lines(u32_view: object) -> List[str]:
    import numpy as np

    return [f"0x{int(x) & 0xFFFFFFFF:08x}" for x in np.asarray(u32_view).ravel()[:4]]


def ignite_system() -> bool:
    """
    Ignite 128-bit Genesis DNA into a float32 Q-reg view; verify against hardware truth.
    """
    try:
        import numpy as np
    except ImportError:
        print("GENESIS: numpy required.", file=sys.stderr)
        return False

    from sk9_bindings import GENESIS_DNA_U32, get_sk9

    sk9 = get_sk9()
    if sk9 is None or not sk9.genesis_ok:
        print("GENESIS: libsk9 not loaded or Phase 0 symbols missing.", file=sys.stderr)
        return False

    genesis_vector = np.zeros(4, dtype=np.float32)
    print("--- CREATION OS: IGNITING GENESIS BLOCK ---")
    if not sk9.genesis_ignite_inplace(genesis_vector):
        print("GENESIS: ignite failed.", file=sys.stderr)
        return False

    is_valid = sk9.genesis_verify(genesis_vector)
    expected = [f"0x{v:08x}" for v in GENESIS_DNA_U32]

    if is_valid:
        print("GENESIS STATUS: 1=1 (COHERENT)")
        dna_hex = _dna_hex_lines(genesis_vector.view(np.uint32))
        print(f"DNA HEX: {dna_hex}")
        if dna_hex != expected:
            print(f"GENESIS: DNA mismatch (got {dna_hex}, want {expected})", file=sys.stderr)
            return False
        return True

    print("GENESIS STATUS: ENTROPY DETECTED. REFORMING...")
    if sk9.rainio_ok:
        sk9.rainio_resonance_inplace(genesis_vector)
    sk9.genesis_ignite_inplace(genesis_vector)
    is_valid = sk9.genesis_verify(genesis_vector)
    dna_hex = _dna_hex_lines(genesis_vector.view(np.uint32))
    print(f"DNA HEX: {dna_hex}")
    if not is_valid or dna_hex != expected:
        print(f"GENESIS: reform failed (coherent={is_valid})", file=sys.stderr)
        return False
    print("GENESIS STATUS: 1=1 (COHERENT)")
    return True


if __name__ == "__main__":
    sys.exit(0 if ignite_system() else 1)
