# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Project bootstrap for ``cos init`` — first-contact scaffolding."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Dict

__all__ = ["bootstrap_project", "run_cos_init"]


def _config_yaml(persona_name: str, spec: Dict[str, Any]) -> str:
    lines = [
        "# Creation OS — project config (lab). Tune for your deployment.",
        f"persona: {persona_name}",
        f"threshold_accept: {spec.get('threshold_accept', 0.2)}",
        f"threshold_abstain: {spec.get('threshold_abstain', 0.6)}",
        f"latency_max_ms: {spec.get('latency_max_ms', 500)}",
        "edge_profile: laptop  # phone | laptop | mcu | automotive",
        "probe_path: null  # set to LSD pickle path when using probes extra",
    ]
    if persona_name == "automotive":
        lines.append("offline: true")
    return "\n".join(lines) + "\n"


_README = """# {name}

Quick start (from this directory):

```bash
pip install 'creation-os[serve]'
cos gate --prompt "What is 2+2?" --response "4" --json
cos serve
```

- ``cos_config.yaml`` — τ and persona defaults (see ``cos.persona.SigmaPersona``).
- ``examples/basic_score.py`` — minimal σ-score script.
- ``evals/sample.jsonl`` — tiny offline-eval sample.
- ``probes/`` — place probe bundles here.

Read **docs/CLAIM_DISCIPLINE.md** before publishing benchmark tables.
"""

_BASIC_SCORE = '''#!/usr/bin/env python3
"""Minimal σ-gate score (offline)."""
from cos.sigma_gate import SigmaGate

def main() -> None:
    gate = SigmaGate()
    s, v = gate.score("What is 2+2?", "4")
    print(f"sigma={s:.6f} verdict={v}")

if __name__ == "__main__":
    main()
'''

_SAMPLE_JSONL = '{"prompt": "What is 2+2?", "response": "4"}\n{"prompt": "Say hello.", "response": "Hello."}\n'


def bootstrap_project(root: Path, persona_name: str) -> None:
    from cos.persona import SigmaPersona

    p = SigmaPersona()
    pn = str(persona_name).strip().lower()
    if pn not in p.personas:
        pn = "enterprise"
    spec = dict(p.personas[pn])
    root.mkdir(parents=True, exist_ok=False)
    (root / "probes").mkdir()
    (root / "evals").mkdir()
    ex = root / "examples"
    ex.mkdir()
    (root / "cos_config.yaml").write_text(_config_yaml(pn, spec), encoding="utf-8")
    (root / "README.md").write_text(_README.format(name=root.name), encoding="utf-8")
    bf = ex / "basic_score.py"
    bf.write_text(_BASIC_SCORE, encoding="utf-8")
    try:
        bf.chmod(bf.stat().st_mode | 0o111)
    except OSError:
        ...
    (root / "evals" / "sample.jsonl").write_text(_SAMPLE_JSONL, encoding="utf-8")


def run_cos_init(*, name: str, persona: str, dest: Path) -> int:
    dest = dest.expanduser().resolve()
    root = dest / str(name).strip()
    if root.exists():
        print(f"cos init: path exists: {root}", file=sys.stderr)
        return 1
    try:
        bootstrap_project(root, persona)
    except OSError as e:
        print(f"cos init: {e}", file=sys.stderr)
        return 1
    print(str(root))
    return 0
