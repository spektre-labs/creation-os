# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v102 σ-Eval-Harness — lm-evaluation-harness backend for creation_os_v101.

Every loglikelihood / generate call is delegated to a subprocess invocation of
`creation_os_v101 --ll` / `creation_os_v101 --gen`, which runs the real
BitNet-b1.58-2B-4T model through bitnet.cpp and returns (loglikelihood,
is_greedy) or (generated_text, sigma_profile).

The σ-channel values are logged alongside every sample so the final JSONL
output is useful for σ vs correctness analysis (RESULTS.md).

Registration: this module is imported automatically when --model creation_os
is passed, provided PYTHONPATH includes benchmarks/v102 (run_eval.sh sets
this).  Internally we register under the name `creation_os` via
`register_model()`.

Status tier: this backend is **measured** (tier M) for loglikelihood scoring;
the σ-gated generation path still goes through greedy argmax which mirrors
lm-eval's default behavior for `generate_until` tasks.
"""

from __future__ import annotations

import json
import os
import subprocess
from typing import Any, List, Optional, Tuple

from lm_eval.api.model import LM
from lm_eval.api.registry import register_model


@register_model("creation_os")
class CreationOSLM(LM):
    """lm-eval backend that pipes every call through creation_os_v101."""

    _DEFAULT_MAX_GEN_TOKS = 256

    def __init__(
        self,
        bridge: str,
        gguf: str,
        sigma_threshold: float = 0.0,
        n_ctx: int = 4096,
        **kwargs: Any,
    ) -> None:
        super().__init__()
        if not os.path.isfile(bridge) or not os.access(bridge, os.X_OK):
            raise FileNotFoundError(
                f"creation_os backend: bridge binary not found / not exec: {bridge}"
            )
        if not os.path.isfile(gguf):
            raise FileNotFoundError(
                f"creation_os backend: GGUF weights not found: {gguf}"
            )
        self.bridge = bridge
        self.gguf = gguf
        self.sigma_threshold = float(sigma_threshold)
        self.n_ctx = int(n_ctx)
        # Log of (task, ctx_hash, sigma_mean) for post-hoc σ vs correctness.
        self._sigma_log: List[dict] = []
        self._proc: Optional[subprocess.Popen] = None  # persistent stdin-mode bridge

    # ------------------------------------------------------------------ #
    # persistent-process bridge (one llama.cpp context for the whole run) #
    # ------------------------------------------------------------------ #
    def _ensure_proc(self) -> subprocess.Popen:
        if self._proc is None or self._proc.poll() is not None:
            self._proc = subprocess.Popen(
                [
                    self.bridge, "--stdin",
                    "--gguf", self.gguf,
                    "--n-ctx", str(self.n_ctx),
                ],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
                bufsize=1,
            )
            banner = self._proc.stdout.readline()
            if not banner.strip().startswith("{"):
                raise RuntimeError(f"creation_os backend: bad banner: {banner!r}")
            import json as _json
            b = _json.loads(banner)
            if not b.get("ready"):
                raise RuntimeError(f"creation_os backend: bridge not ready: {banner!r}")
        return self._proc

    def _rpc(self, request: dict) -> dict:
        proc = self._ensure_proc()
        proc.stdin.write(json.dumps(request) + "\n")
        proc.stdin.flush()
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError("creation_os backend: bridge closed stdout unexpectedly")
        return json.loads(line)

    def __del__(self) -> None:
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
    # required LM interface                                              #
    # ------------------------------------------------------------------ #
    def loglikelihood(self, requests, disable_tqdm: bool = False):
        from tqdm import tqdm

        out: List[Tuple[float, bool]] = []
        it = requests if disable_tqdm else tqdm(requests, desc="cos:ll")
        for req in it:
            ctx, cont = req.args
            ll, is_greedy, sigma_mean = self._ll_one(ctx, cont)
            self._sigma_log.append(
                {"kind": "ll", "sigma_mean": sigma_mean, "ll": ll, "greedy": is_greedy}
            )
            out.append((ll, bool(is_greedy)))
        return out

    def loglikelihood_rolling(self, requests, disable_tqdm: bool = False):
        raise NotImplementedError(
            "creation_os backend: loglikelihood_rolling not yet implemented"
        )

    def generate_until(self, requests, disable_tqdm: bool = False):
        from tqdm import tqdm

        out: List[str] = []
        it = requests if disable_tqdm else tqdm(requests, desc="cos:gen")
        for req in it:
            ctx, gen_kwargs = req.args
            until = list(gen_kwargs.get("until", []) or [])
            max_gen = int(gen_kwargs.get("max_gen_toks", self._DEFAULT_MAX_GEN_TOKS))
            text, profile, abstained = self._gen_one(ctx, until, max_gen)
            sigma_mean = sum(profile) / len(profile) if profile else 0.0
            self._sigma_log.append(
                {
                    "kind": "gen",
                    "sigma_mean": sigma_mean,
                    "abstained": abstained,
                    "n_chars": len(text),
                }
            )
            out.append(text)
        return out

    # ------------------------------------------------------------------ #
    # subprocess helpers                                                 #
    # ------------------------------------------------------------------ #
    def _ll_one(self, ctx: str, cont: str) -> Tuple[float, bool, float]:
        j = self._rpc({"op": "ll", "ctx": ctx, "cont": cont})
        if "error" in j:
            raise RuntimeError(f"creation_os_v101 ll error: {j['error']}")
        return float(j["loglikelihood"]), bool(j["is_greedy"]), float(j.get("sigma_mean", 0.0))

    def _gen_one(
        self, ctx: str, until: List[str], max_tokens: int
    ) -> Tuple[str, List[float], bool]:
        req = {"op": "gen", "ctx": ctx, "max_tokens": int(max_tokens)}
        if self.sigma_threshold > 0:
            req["sigma_threshold"] = float(self.sigma_threshold)
        j = self._rpc(req)
        if "error" in j:
            raise RuntimeError(f"creation_os_v101 gen error: {j['error']}")
        text = str(j["text"])
        # lm-eval expects the generator to respect `until` stop sequences —
        # truncate client-side if the stdin-mode bridge overshoots.
        for s in until:
            idx = text.find(s)
            if idx >= 0:
                text = text[:idx]
                break
        profile = list(j.get("sigma_profile", [])) or []
        return text, [float(x) for x in profile], bool(j.get("abstained", False))

    # ------------------------------------------------------------------ #
    # diagnostics (optional; saved next to results)                      #
    # ------------------------------------------------------------------ #
    def flush_sigma_log(self, path: Optional[str] = None) -> None:
        if not path:
            return
        with open(path, "w") as fh:
            for row in self._sigma_log:
                fh.write(json.dumps(row) + "\n")
