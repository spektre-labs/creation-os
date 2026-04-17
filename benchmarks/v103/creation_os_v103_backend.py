# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v103 σ-Eval-Harness (τ-sweep extension).

Thin subclass of the v102 `CreationOSLM` backend that additionally serialises,
per loglikelihood call, the full 8-channel σ-profile plus the max-per-token σ
observed during teacher-forced decoding of the continuation.

Why a sidecar (not a second model run):

    The v102 2026-04-17 run produced correct ll values but only stored
    `sigma_mean` in an in-memory list — the lm-eval sample JSONL has no σ
    column.  Computing an abstain-threshold sweep (τ ∈ [0, 1]) is a purely
    post-hoc operation on σ; no new forward pass is required.  We therefore
    re-run the eval ONCE with this σ-persisting backend, then let
    `compute_rc_metrics.py` sweep τ offline.  Total cost ≈ one v102 run
    (~2 h wall) vs. 10× that if τ were baked into the forward pass.

Registered model name: `creation_os_v103`  (kept distinct from
`creation_os` so both backends coexist).

Sidecar file:
    For each task run, one JSONL file is written to
        COS_V103_SIGMA_SIDECAR (env var) OR
        <output_path>/samples_<task>_sigma.jsonl
    with shape:
        { "task": str, "doc_index": int, "cand_index": int,
          "ll": float, "is_greedy": bool,
          "sigma_mean": float, "sigma_max_token": float,
          "sigma_profile": [e, m, tk, tl, ls, pm, ne, std] }
    `doc_index` is the running index of the document in request order;
    `cand_index` is the index of this candidate continuation within that
    document (0-based).  Both are enough to rejoin with lm-eval's
    `samples_<task>_*.jsonl` after the run.
"""

from __future__ import annotations

import json
import os
import subprocess
from typing import Any, List, Optional, Tuple

from lm_eval.api.model import LM
from lm_eval.api.registry import register_model


@register_model("creation_os_v103")
class CreationOSV103LM(LM):
    """v103 backend — identical to v102 in semantics, stricter σ logging."""

    _DEFAULT_MAX_GEN_TOKS = 256

    def __init__(
        self,
        bridge: str,
        gguf: str,
        n_ctx: int = 4096,
        sigma_sidecar: Optional[str] = None,
        task_tag: str = "mixed",
        **kwargs: Any,
    ) -> None:
        super().__init__()
        if not os.path.isfile(bridge) or not os.access(bridge, os.X_OK):
            raise FileNotFoundError(f"v103 backend: bridge not found / not exec: {bridge}")
        if not os.path.isfile(gguf):
            raise FileNotFoundError(f"v103 backend: gguf not found: {gguf}")
        self.bridge = bridge
        self.gguf = gguf
        self.n_ctx = int(n_ctx)
        self.task_tag = task_tag
        self.sigma_sidecar_path = (
            sigma_sidecar
            or os.environ.get("COS_V103_SIGMA_SIDECAR")
        )
        self._sidecar_fh = None
        self._proc: Optional[subprocess.Popen] = None
        # counters for (doc_index, cand_index); one doc = one "ctx" in practice.
        self._last_ctx: Optional[str] = None
        self._doc_index = -1
        self._cand_index = 0

    # ------------------------------------------------------------------ #
    def _ensure_sidecar(self):
        if self._sidecar_fh is None and self.sigma_sidecar_path:
            os.makedirs(os.path.dirname(self.sigma_sidecar_path) or ".", exist_ok=True)
            self._sidecar_fh = open(self.sigma_sidecar_path, "a", buffering=1)
        return self._sidecar_fh

    def _ensure_proc(self) -> subprocess.Popen:
        if self._proc is None or self._proc.poll() is not None:
            self._proc = subprocess.Popen(
                [self.bridge, "--stdin", "--gguf", self.gguf, "--n-ctx", str(self.n_ctx)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                text=True, bufsize=1,
            )
            banner = self._proc.stdout.readline()
            if not banner.strip().startswith("{"):
                raise RuntimeError(f"v103 backend: bad banner: {banner!r}")
            b = json.loads(banner)
            if not b.get("ready"):
                raise RuntimeError(f"v103 backend: bridge not ready: {banner!r}")
        return self._proc

    def _rpc(self, request: dict) -> dict:
        proc = self._ensure_proc()
        proc.stdin.write(json.dumps(request) + "\n")
        proc.stdin.flush()
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError("v103 backend: bridge closed stdout unexpectedly")
        return json.loads(line)

    def __del__(self) -> None:
        try:
            if self._sidecar_fh is not None:
                self._sidecar_fh.close()
        except Exception:
            pass
        try:
            if self._proc is not None and self._proc.poll() is None:
                try:
                    self._proc.stdin.write('{"op":"quit"}\n')
                    self._proc.stdin.flush()
                except Exception:
                    pass
                self._proc.terminate()
                try:
                    self._proc.wait(timeout=2)
                except Exception:
                    self._proc.kill()
        except Exception:
            pass

    # ------------------------------------------------------------------ #
    def loglikelihood(self, requests, disable_tqdm: bool = False):
        from tqdm import tqdm

        out: List[Tuple[float, bool]] = []
        it = requests if disable_tqdm else tqdm(requests, desc="cos_v103:ll")
        sidecar = self._ensure_sidecar()
        for req in it:
            ctx, cont = req.args
            if ctx != self._last_ctx:
                self._last_ctx = ctx
                self._doc_index += 1
                self._cand_index = 0
            else:
                self._cand_index += 1

            j = self._rpc({"op": "ll", "ctx": ctx, "cont": cont})
            if "error" in j:
                raise RuntimeError(f"v103: --ll error: {j['error']}")
            ll = float(j["loglikelihood"])
            greedy = bool(j["is_greedy"])
            prof = [float(x) for x in j.get("sigma_profile", [0.0] * 8)]
            sm = float(j.get("sigma_mean", 0.0))
            sx = float(j.get("sigma_max_token", sm))

            if sidecar is not None:
                sidecar.write(json.dumps({
                    "task": self.task_tag,
                    "doc_index": self._doc_index,
                    "cand_index": self._cand_index,
                    "ll": ll,
                    "is_greedy": greedy,
                    "sigma_mean": sm,
                    "sigma_max_token": sx,
                    "sigma_profile": prof,
                }) + "\n")
            out.append((ll, greedy))
        return out

    def loglikelihood_rolling(self, requests, disable_tqdm: bool = False):
        raise NotImplementedError("v103: loglikelihood_rolling not implemented")

    def generate_until(self, requests, disable_tqdm: bool = False):
        # v103 is a loglikelihood-only sweep; MC tasks don't call this.
        raise NotImplementedError("v103: generate_until not used in τ-sweep")
