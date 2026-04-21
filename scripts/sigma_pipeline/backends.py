# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""LLM backends for the σ-pipeline.

Two concrete implementations, one contract:

    class Backend:
        def step(self, prompt: str, accumulated: str) -> TokenStep

``StubBackend`` emits a deterministic σ trace from a fixed seed so the
pipeline is fully testable without BitNet installed.  ``BridgeBackend``
drives ``./creation_os_v101 --stdin`` (the real BitNet/llama-cpp
subprocess) via the existing JSON-RPC protocol — one token at a time by
calling ``gen`` with ``max_tokens=1``.

Both implementations return a ``TokenStep`` with:

    step        : int            # 0-based generation step index
    token_text  : str            # piece emitted this step
    sigma_mean  : float          # mean σ over this step
    sigma_max   : float          # max σ over this step
    sigma_product: float         # product-style aggregate
    sigma_profile: list[float]   # 8 channels (entropy_norm, margin, top_k, tail,
                                 # logit_spread, p_max, n_effective, logit_std)
    eos         : bool           # model emitted end-of-generation
"""

from __future__ import annotations

import dataclasses
import hashlib
import json
import math
import os
import subprocess
from typing import List, Optional, Protocol, Sequence


@dataclasses.dataclass
class TokenStep:
    step: int
    token_text: str
    sigma_mean: float
    sigma_max: float
    sigma_product: float
    sigma_profile: List[float]
    eos: bool = False


class Backend(Protocol):
    """Minimum surface a backend must implement."""

    def step(self, prompt: str, accumulated: str) -> TokenStep: ...

    def close(self) -> None: ...


# --------------------------------------------------------------------------
# StubBackend
# --------------------------------------------------------------------------


class StubBackend:
    """Deterministic fake LLM.

    Emits a fixed sequence of tokens with a σ trace derived from a SHA-256
    hash of (prompt + accumulated).  The σ values are stable across runs and
    OS/arch (hashlib is not platform-dependent), so a smoke test can pin
    exact numbers.

    The default token sequence is an English sentence terminated with ``.``
    so the σ-speculative segment-boundary predicate fires.  Callers may
    override ``tokens`` and ``sigma_trace``; the sequence repeats indefinitely.
    """

    _DEFAULT_TOKENS: Sequence[str] = (
        "The ", "answer ", "is ", "forty", "-two", ".", "\n",
    )
    _DEFAULT_SIGMA: Sequence[float] = (
        # low low low  high  mid  low  low
        0.12, 0.18, 0.22, 0.78, 0.45, 0.15, 0.10,
    )

    def __init__(
        self,
        tokens: Optional[Sequence[str]] = None,
        sigma_trace: Optional[Sequence[float]] = None,
        max_steps: int = 64,
        seed_salt: str = "cos-stub-v1",
    ) -> None:
        self._tokens = list(tokens or self._DEFAULT_TOKENS)
        self._trace = list(sigma_trace or self._DEFAULT_SIGMA)
        if len(self._tokens) != len(self._trace):
            raise ValueError("tokens and sigma_trace must be same length")
        self._max = int(max_steps)
        self._salt = seed_salt
        self._step = 0
        self._closed = False

    def step(self, prompt: str, accumulated: str) -> TokenStep:
        if self._closed:
            raise RuntimeError("StubBackend: closed")
        if self._step >= self._max:
            return TokenStep(
                step=self._step, token_text="", sigma_mean=0.0,
                sigma_max=0.0, sigma_product=0.0,
                sigma_profile=[0.0] * 8, eos=True,
            )
        idx = self._step % len(self._tokens)
        tok = self._tokens[idx]
        base = self._trace[idx]
        # Add a tiny deterministic jitter from the prompt+accumulated hash so
        # the trace is prompt-dependent but reproducible.
        h = hashlib.sha256(
            (self._salt + "|" + prompt + "|" + accumulated).encode("utf-8")
        ).digest()
        jitter = (h[self._step % 32] / 255.0 - 0.5) * 0.04   # ±0.02
        sig = max(0.0, min(1.0, base + jitter))
        profile = [sig, sig * 0.9, sig * 1.1, sig * 0.8,
                   sig * 0.85, sig * 0.95, sig, sig * 0.9]
        # Clamp each channel to [0, 1].
        profile = [max(0.0, min(1.0, x)) for x in profile]
        # Product: 1 - exp(-<σ channels>) geometric-mean style — stable and
        # always ≤ max channel; matches the v227 σ_product convention at
        # zero-order approximation.
        gm = math.exp(sum(math.log(max(1e-9, 1.0 - x)) for x in profile)
                      / len(profile))
        sigma_product = max(0.0, min(1.0, 1.0 - gm))
        eos = tok == "" or (self._step + 1) >= self._max
        out = TokenStep(
            step=self._step,
            token_text=tok,
            sigma_mean=float(sum(profile) / len(profile)),
            sigma_max=float(max(profile)),
            sigma_product=sigma_product,
            sigma_profile=profile,
            eos=eos,
        )
        self._step += 1
        return out

    def close(self) -> None:
        self._closed = True


# --------------------------------------------------------------------------
# BridgeBackend
# --------------------------------------------------------------------------


class BridgeBackend:
    """Drives ``./creation_os_v101 --stdin`` one token at a time.

    One call to :py:meth:`step` issues one ``{"op":"gen","max_tokens":1,
    "ctx":"<prompt+accumulated>"}`` request and reads one JSON line
    response.  The bridge returns aggregate σ_profile for the single
    token generated, so σ_mean == σ_max per step.  For richer per-step
    statistics (e.g. σ_product aggregated over a token window), call
    step() repeatedly and aggregate client-side.

    This backend is used by cos_chat and the cost-measurement driver when
    a BitNet gguf + compiled bridge are available.  The stub backend is
    the CI default.
    """

    def __init__(self, bridge: str, gguf: str, n_ctx: int = 4096) -> None:
        if not os.path.isfile(bridge) or not os.access(bridge, os.X_OK):
            raise FileNotFoundError(
                f"BridgeBackend: bridge missing / not exec: {bridge}"
            )
        if not os.path.isfile(gguf):
            raise FileNotFoundError(f"BridgeBackend: gguf missing: {gguf}")
        self._proc: Optional[subprocess.Popen] = subprocess.Popen(
            [bridge, "--stdin", "--gguf", gguf, "--n-ctx", str(n_ctx)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        banner = self._proc.stdout.readline()
        b = json.loads(banner)
        if not b.get("ready"):
            raise RuntimeError(f"BridgeBackend: bridge not ready: {banner!r}")
        self._step = 0

    def step(self, prompt: str, accumulated: str) -> TokenStep:
        if self._proc is None:
            raise RuntimeError("BridgeBackend: already closed")
        req = {
            "op": "gen",
            "ctx": prompt + accumulated,
            "max_tokens": 1,
            "sigma_threshold": 0.0,  # never abstain inside the bridge;
                                     # the Python policy does that.
        }
        self._proc.stdin.write(json.dumps(req) + "\n")
        self._proc.stdin.flush()
        line = self._proc.stdout.readline()
        if not line:
            raise RuntimeError("BridgeBackend: bridge closed stdout")
        resp = json.loads(line)
        if "error" in resp:
            raise RuntimeError(f"BridgeBackend: gen error: {resp['error']}")
        text = resp.get("text", "")
        profile = list(resp.get("sigma_profile", [0.0] * 8))
        if len(profile) < 8:
            profile += [0.0] * (8 - len(profile))
        profile = [max(0.0, min(1.0, float(x))) for x in profile[:8]]
        sm = float(sum(profile) / len(profile))
        mx = float(max(profile))
        gm = math.exp(sum(math.log(max(1e-9, 1.0 - x)) for x in profile)
                      / len(profile))
        sigma_product = max(0.0, min(1.0, 1.0 - gm))
        eos = (text == "") or bool(resp.get("abstained", False))
        out = TokenStep(
            step=self._step,
            token_text=text,
            sigma_mean=sm,
            sigma_max=mx,
            sigma_product=sigma_product,
            sigma_profile=profile,
            eos=eos,
        )
        self._step += 1
        return out

    def close(self) -> None:
        if self._proc is not None:
            try:
                self._proc.stdin.write('{"op":"quit"}\n')
                self._proc.stdin.flush()
            except Exception:
                pass
            try:
                self._proc.terminate()
                self._proc.wait(timeout=2)
            except Exception:
                try:
                    self._proc.kill()
                except Exception:
                    pass
            self._proc = None


# --------------------------------------------------------------------------
# LlamaCliBackend — one-shot llama-cli / BitNet (matches cos-chat C bridge)
# --------------------------------------------------------------------------


class LlamaCliBackend:
    """Runs ``CREATION_OS_BITNET_EXE`` once per prompt (``-m`` model from env).

    Emits the entire completion as a single terminal token step so the
    existing ``GenerateUntilEngine`` loop stays well-defined.  σ is a
    lightweight length / punctuation heuristic unless
    ``COS_LLAMA_CLI_SIGMA`` is set to a float in ``[0, 1]``.
    """

    def __init__(self) -> None:
        exe = os.environ.get("CREATION_OS_BITNET_EXE", "").strip()
        model = os.environ.get("CREATION_OS_BITNET_MODEL", "").strip()
        if not exe or not model:
            raise OSError(
                "LlamaCliBackend requires CREATION_OS_BITNET_EXE and "
                "CREATION_OS_BITNET_MODEL in the environment",
            )
        self._exe = exe
        self._model = model
        self._buf: str = ""
        self._closed = False

    @staticmethod
    def _heuristic_sigma(text: str) -> float:
        fixed = os.environ.get("COS_LLAMA_CLI_SIGMA", "").strip()
        if fixed:
            try:
                v = float(fixed)
                return max(0.0, min(1.0, v))
            except ValueError:
                pass
        t = text.rstrip()
        n = len(t)
        if n == 0:
            return 1.0
        q = t.count("?")
        short_num = n <= 6 and any(ch.isdigit() for ch in t) and all(
            (ch.isdigit() or ch in " \t-+./") for ch in t
        )
        base = 0.18 + n * 0.00035
        if short_num:
            base *= 0.35
        base += 0.06 * q
        return max(0.05, min(0.95, base))

    def _run_cli(self, prompt: str) -> str:
        head = [self._exe, "--no-perf", "-m", self._model]
        extra: list[str] = []
        th = os.environ.get("CREATION_OS_BITNET_THREADS", "").strip()
        if th:
            extra += ["-t", th]
        tail = [
            "-n",
            os.environ.get("CREATION_OS_BITNET_N_PREDICT", "128"),
            "-ngl",
            "0",
            "-p",
            prompt,
        ]
        proc = subprocess.run(
            head + extra + tail,
            capture_output=True,
            text=True,
            timeout=int(os.environ.get("CREATION_OS_BITNET_TIMEOUT_SEC", "600")),
            check=False,
        )
        if proc.returncode != 0:
            err = (proc.stderr or "")[:400]
            raise RuntimeError(f"llama-cli failed rc={proc.returncode}: {err}")
        out = proc.stdout or ""
        # Strip common ANSI escapes (best-effort; matches C helper intent).
        esc = "\033["
        while esc in out:
            i = out.index(esc)
            j = out.find("m", i + 2)
            if j == -1:
                break
            out = out[:i] + out[j + 1 :]
        return out.strip()

    def step(self, prompt: str, accumulated: str) -> TokenStep:
        if self._closed:
            raise RuntimeError("LlamaCliBackend: closed")
        if accumulated != "":
            return TokenStep(
                step=0,
                token_text="",
                sigma_mean=0.05,
                sigma_max=0.05,
                sigma_product=0.05,
                sigma_profile=[0.05] * 8,
                eos=True,
            )
        # Fresh ``GenerateUntilEngine.run`` starts with accumulated == "".
        # RETHINK rounds change ``prompt``; re-query llama-cli each time.
        self._buf = self._run_cli(prompt)
        sig = self._heuristic_sigma(self._buf)
        prof = [sig] * 8
        sm = mx = sig
        gm = math.exp(sum(math.log(max(1e-9, 1.0 - x)) for x in prof) / len(prof))
        sigma_product = max(0.0, min(1.0, 1.0 - gm))
        return TokenStep(
            step=0,
            token_text=self._buf,
            sigma_mean=sm,
            sigma_max=mx,
            sigma_product=sigma_product,
            sigma_profile=prof,
            eos=True,
        )

    def close(self) -> None:
        self._closed = True
