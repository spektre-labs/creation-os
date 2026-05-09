# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Gemma-family **host** evals: TruthfulQA sweep (local weights) + σ-discrimination lab (API or mock).

**Tokens:** use ``HUGGINGFACE_HUB_TOKEN`` or ``HF_TOKEN`` only — **never** hard-code secrets in source.

- :func:`run_truthfulqa_gemma_n` — local ``transformers`` run; writes under ``results/``.
- :class:`GemmaEval` — short STEM probe list; scores **question vs model answer** and compares
  σ(reference correct) vs σ(reference wrong). This is a **lab** readout (not GPQA Diamond /
  AIME harness parity). External headline scores and pricing for Gemma 4 belong in cited
  third-party reports only — see ``docs/CLAIM_DISCIPLINE.md``.

Checkpoint file ``results/gemma_n30.checkpoint.json`` every ``checkpoint_every`` **questions**
for the TruthfulQA path.
"""
from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from cos.calibrate import behavioral_calibration_table
from cos.eval.truthfulqa import load_truthfulqa_rows
from cos.sigma_gate import SigmaGate

try:
    from huggingface_hub import InferenceClient

    _HAS_HF_INFERENCE = True
except ImportError:  # pragma: no cover
    InferenceClient = None  # type: ignore[misc, assignment]
    _HAS_HF_INFERENCE = False

__all__ = [
    "GPQA_SAMPLE",
    "GemmaEval",
    "default_gemma_eval_output_path",
    "run_truthfulqa_gemma_n",
]


def _repo_root() -> Path:
    env = (os.environ.get("CREATION_OS_ROOT") or "").strip()
    if env:
        return Path(env).expanduser().resolve()
    here = Path(__file__).resolve()
    for base in (Path.cwd().resolve(), *here.parents):
        if (base / "creation_os_v2.c").is_file():
            return base
    return here.parents[3]


def _save_json(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, default=str), encoding="utf-8")


def _verdict_label(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


def resolve_hf_token(explicit: Optional[str] = None) -> str:
    """Return HF token from *explicit* or environment; raise if missing."""
    tok = (explicit or os.environ.get("HUGGINGFACE_HUB_TOKEN") or os.environ.get("HF_TOKEN") or "").strip()
    if not tok:
        raise RuntimeError(
            "Gemma Inference API needs HUGGINGFACE_HUB_TOKEN or HF_TOKEN (or pass hf_token=). "
            "Do not hard-code secrets.",
        )
    return tok


# Synthetic STEM probes (not official GPQA/AIME items). For discrimination σ(correct ref)
# vs σ(wrong ref) only — **not** a claim of benchmark replication.
GPQA_SAMPLE: List[Dict[str, str]] = [
    {"q": "In quantum mechanics, the commutator [x, p] equals?", "correct": "iℏ", "wrong": "0"},
    {"q": "What is the Schwarzschild radius (non-rotating black hole)?", "correct": "2GM/c²", "wrong": "GM/c"},
    {
        "q": "The Krebs cycle directly produces how many ATP per glucose (substrate-level)?",
        "correct": "2 ATP (NADH/FADH2 drive oxidative phosphorylation separately)",
        "wrong": "36 ATP directly in the cycle",
    },
    {"q": "Speed of light in vacuum is denoted by?", "correct": "c", "wrong": "g"},
    {"q": "The four DNA bases are?", "correct": "A, T, G, C", "wrong": "A, U, G, Z"},
    {"q": "State Ohm's law for a resistor.", "correct": "V = IR", "wrong": "V = I/R"},
    {"q": "pH of a neutral aqueous solution at 25°C (standard).", "correct": "7", "wrong": "0"},
    {"q": "Newton's second law (one-dimensional).", "correct": "F = ma", "wrong": "F = m/a"},
    {"q": "Ideal gas law (single form).", "correct": "PV = nRT", "wrong": "PV = nR/T"},
    {"q": "Planck–Einstein relation for photon energy.", "correct": "E = hν", "wrong": "E = h/ν"},
    {"q": "Which organelle is the primary site of aerobic ATP synthesis in eukaryotes?", "correct": "mitochondrion", "wrong": "Golgi apparatus"},
    {"q": "Primary oxygen-evolving process in plants/algae?", "correct": "photosynthesis (light reactions)", "wrong": "fermentation only"},
    {"q": "Hemoglobin binds oxygen at which metal center?", "correct": "iron (heme)", "wrong": "magnesium porphyrin"},
    {"q": "Entropy of an isolated system tends to (second law, coarse statement).", "correct": "increase or stay constant", "wrong": "always decrease"},
    {"q": "Energy stored in a capacitor (ideal).", "correct": "½ C V²", "wrong": "C V"},
    {"q": "Bayes' theorem numerator for P(A|B) up to proportionality.", "correct": "P(B|A) P(A)", "wrong": "P(A) / P(B|A)"},
    {"q": "TCP vs UDP — which provides reliable, in-order delivery (typical stack)?", "correct": "TCP", "wrong": "UDP"},
    {"q": "Gradient descent step direction on loss L(w).", "correct": "opposite to ∇L", "wrong": "same as ∇L"},
    {"q": "Softmax over logits produces?", "correct": "a probability simplex", "wrong": "unbounded scores"},
    {"q": "SAT was the first problem shown NP-complete (Cook–Levin, standard statement).", "correct": "Boolean satisfiability (SAT)", "wrong": "shortest path"},
    {"q": "Turing machine infinite tape model — head reads/writes?", "correct": "one cell at a time", "wrong": "entire tape at once"},
    {"q": "CRISPR–Cas9 primarily uses guide RNA to?", "correct": "target a DNA sequence for cleavage", "wrong": "transcribe mRNA without DNA"},
    {"q": "Natural numbers' cardinality vs reals (Cantor).", "correct": "|ℕ| < |ℝ|", "wrong": "|ℕ| = |ℝ|"},
    {"q": "Which antibiotic resistance mechanism includes enzymatic inactivation?", "correct": "β-lactamase (example)", "wrong": "only efflux with zero enzymes"},
    {"q": "Faraday's law relates to?", "correct": "EMF from changing magnetic flux", "wrong": "static Coulomb force only"},
    {"q": "Capacitors in parallel: equivalent capacitance.", "correct": "sum of capacitances", "wrong": "harmonic mean of capacitances"},
    {"q": "siRNA typically silences genes via?", "correct": "RISC-mediated mRNA cleavage/translational block", "wrong": "direct DNA replication"},
    {"q": "Sn1 vs Sn2 — Sn1 rate law (idealized unimolecular step).", "correct": "rate depends on substrate only", "wrong": "always second order in nucleophile only"},
    {"q": "Hubble's law relates redshift-distance to?", "correct": "expansion (Hubble parameter)", "wrong": "Earth-centric fixed sphere"},
    {"q": "LLM next-token distribution is typically?", "correct": "categorical from softmax over vocabulary", "wrong": "a single deterministic integer with no distribution"},
]


def default_gemma_eval_output_path() -> Path:
    root = _repo_root()
    return root / "eval_results" / "gemma_eval.json"


class GemmaEval:
    """Score Gemma (or mock) answers with :class:`~cos.sigma_gate.SigmaGate` + reference probes."""

    def __init__(
        self,
        gate: Optional[SigmaGate] = None,
        hf_token: Optional[str] = None,
        model: str = "google/gemma-2-9b-it",
        *,
        mock: bool = False,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.model = str(model)
        self.results: List[Dict[str, Any]] = []
        self._last_summary: Dict[str, Any] = {}
        self.client: Any = None
        self._mock = bool(mock)

        if self._mock:
            return
        if not _HAS_HF_INFERENCE:
            return
        try:
            tok = resolve_hf_token(hf_token)
        except RuntimeError:
            return
        try:
            self.client = InferenceClient(model=self.model, token=tok)
        except Exception:  # pragma: no cover
            self.client = None

    def generate(self, prompt: str, max_tokens: int = 256) -> str:
        """Generate one continuation (Inference API) or deterministic mock."""
        if self._mock or not self.client:
            return f"[mock response for: {prompt[:60]}]"
        try:
            out = self.client.text_generation(
                str(prompt),
                max_new_tokens=int(max_tokens),
            )
            return str(out).strip()
        except Exception as exc:  # pragma: no cover
            return f"[inference error: {exc}]"

    def eval_question(self, question: str, correct: str, wrong: str) -> Dict[str, Any]:
        """σ(question, model_answer) plus σ vs reference strings (discrimination lab)."""
        response = self.generate(question)
        σ, verdict = self.gate.score(question, response)
        σ_correct, _ = self.gate.score(question, correct)
        σ_wrong, _ = self.gate.score(question, wrong)
        disc = float(σ_wrong) - float(σ_correct)
        return {
            "question": question[:160],
            "response_preview": str(response)[:240],
            "σ": round(float(σ), 4),
            "verdict": _verdict_label(verdict),
            "σ_correct_ref": round(float(σ_correct), 4),
            "σ_wrong_ref": round(float(σ_wrong), 4),
            "would_catch_hallucination": float(σ_wrong) > float(σ_correct),
            "discrimination": round(disc, 4),
        }

    def run(
        self,
        questions: Optional[List[Dict[str, str]]] = None,
        n: int = 30,
    ) -> Dict[str, Any]:
        """Run up to *n* probes; always reports aggregates (including negative / zero catch rate)."""
        qs = (questions or GPQA_SAMPLE)[: max(0, int(n))]
        self.results = []
        if not qs:
            self._last_summary = {
                "model": self.model,
                "n": 0,
                "mock_mode": self._mock or not bool(self.client),
                "has_huggingface_hub": _HAS_HF_INFERENCE,
                "hallucination_catch_rate": 0.0,
                "avg_discrimination": 0.0,
                "avg_σ": 0.0,
                "results": [],
                "conclusion": "No questions — n=0 or empty list.",
                "disclaimer": "Lab σ readout only; not harness MMLU/GPQA/AIME. See docs/CLAIM_DISCIPLINE.md.",
            }
            return self._last_summary

        for row in qs:
            self.results.append(
                self.eval_question(str(row["q"]), str(row["correct"]), str(row["wrong"])),
            )

        catches = sum(1 for r in self.results if r["would_catch_hallucination"])
        avg_disc = sum(float(r["discrimination"]) for r in self.results) / len(self.results)
        avg_s = sum(float(r["σ"]) for r in self.results) / len(self.results)
        catch_rate = catches / len(self.results)

        self._last_summary = {
            "model": self.model,
            "n": len(self.results),
            "mock_mode": self._mock or not bool(self.client),
            "has_huggingface_hub": _HAS_HF_INFERENCE,
            "hallucination_catch_rate": round(catch_rate, 4),
            "avg_discrimination": round(avg_disc, 4),
            "avg_σ": round(avg_s, 4),
            "results": self.results,
            "conclusion": (
                f"σ-gate reference discrimination (wrong ref > correct ref) holds on "
                f"{catches}/{len(self.results)} probes. Avg discrimination Δσ={avg_disc:.4f}."
            ),
            "disclaimer": (
                "Synthetic probes only; external Gemma leaderboard / pricing claims require "
                "third-party citations — never merge with this lab JSON as one headline."
            ),
        }
        return self._last_summary

    def save(self, path: Optional[Union[str, Path]] = None) -> Path:
        """Write JSON under *path* (default: ``eval_results/gemma_eval.json`` from repo root)."""
        out = Path(path) if path else default_gemma_eval_output_path()
        out.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "model": self.model,
            "timestamp": time.time(),
            "summary": self._last_summary,
            "results": self.results,
        }
        _save_json(out, payload)
        return out


def run_truthfulqa_gemma_n(
    *,
    model_id: str = "google/gemma-2b-it",
    n_samples: int = 30,
    checkpoint_every: int = 100,
    max_questions: Optional[int] = None,
    dry_run: bool = False,
    data_path: Optional[Path] = None,
    out_path: Optional[Path] = None,
) -> Dict[str, Any]:
    """Generate ``n_samples`` responses per TruthfulQA row and score with :class:`SigmaGate`."""
    root = _repo_root()
    out = out_path or (root / "results" / "gemma_n30.json")
    ckpt = out.with_name(out.stem + ".checkpoint.json")
    rows = load_truthfulqa_rows(data_path)
    gate = SigmaGate()
    records: List[Dict[str, Any]] = []
    meta = {
        "model_id": model_id,
        "n_samples": int(n_samples),
        "checkpoint_every_questions": int(checkpoint_every),
        "dry_run": bool(dry_run),
        "started_at": time.time(),
        "disclaimer": "Host metrics only; archive machine metadata for reproducibility.",
    }

    if dry_run:
        n_samples = min(3, int(n_samples))
        rows = rows[: min(2, len(rows))]
        for row in rows:
            prompt = str(row.get("prompt", ""))
            for _i in range(n_samples):
                hyp = f"(dry-run) echo: {prompt[:40]}"
                sigma = float(gate.compute_sigma(None, None, prompt, hyp))
                ver = str(gate._verdict(sigma))
                ok = str(row.get("expected", "")).lower() in hyp.lower()
                records.append(
                    {
                        "prompt": prompt,
                        "hypothesis": hyp,
                        "sigma": sigma,
                        "verdict": ver,
                        "answered": ver != "ABSTAIN",
                        "abstain": ver == "ABSTAIN",
                        "correct": ok and ver != "ABSTAIN",
                        "oracle_correct": ok,
                        "confidence": max(0.0, min(1.0, 1.0 - sigma)),
                    },
                )
        meta["behavioral"] = behavioral_calibration_table(records)
        meta["records_n"] = len(records)
        meta["finished_at"] = time.time()
        payload = {"meta": meta, "records": records}
        _save_json(out, payload)
        return payload

    resolve_hf_token()
    try:
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError as e:
        raise ImportError(
            "gemma_eval requires torch+transformers in this environment.",
        ) from e

    tok = resolve_hf_token()
    tokenizer = AutoTokenizer.from_pretrained(model_id, token=tok)
    model = AutoModelForCausalLM.from_pretrained(
        model_id,
        token=tok,
        device_map="auto",
        torch_dtype=torch.float16,
    )
    model.eval()

    mq = len(rows) if max_questions is None else min(int(max_questions), len(rows))
    for qi, row in enumerate(rows[:mq]):
        prompt = str(row.get("prompt", ""))
        for _si in range(int(n_samples)):
            inputs = tokenizer(prompt, return_tensors="pt").to(model.device)
            with torch.no_grad():
                out_ids = model.generate(
                    **inputs,
                    max_new_tokens=64,
                    do_sample=True,
                    temperature=0.7,
                )
            hyp = tokenizer.decode(out_ids[0], skip_special_tokens=True)
            if hyp.startswith(prompt):
                hyp = hyp[len(prompt) :].strip()
            sigma = float(gate.compute_sigma(None, None, prompt, hyp))
            ver = str(gate._verdict(sigma))
            ok = str(row.get("expected", "")).lower() in hyp.lower()
            records.append(
                {
                    "prompt": prompt,
                    "hypothesis": hyp,
                    "sigma": sigma,
                    "verdict": ver,
                    "answered": ver != "ABSTAIN",
                    "abstain": ver == "ABSTAIN",
                    "correct": ok and ver != "ABSTAIN",
                    "oracle_correct": ok,
                    "confidence": max(0.0, min(1.0, 1.0 - sigma)),
                },
            )
        if checkpoint_every > 0 and (qi + 1) % int(checkpoint_every) == 0:
            _save_json(
                ckpt,
                {
                    "meta": {**meta, "checkpoints_at_question": qi + 1},
                    "records": records,
                },
            )

    meta["behavioral"] = behavioral_calibration_table(records)
    meta["records_n"] = len(records)
    meta["finished_at"] = time.time()
    payload = {"meta": meta, "records": records}
    _save_json(out, payload)
    return payload


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Gemma TruthfulQA multi-sample eval (host).")
    ap.add_argument("--dry-run", action="store_true", help="No weights; writes stub JSON.")
    ap.add_argument("--model", default="google/gemma-2b-it")
    ap.add_argument("--n-samples", type=int, default=30)
    ap.add_argument("--checkpoint-every", type=int, default=100)
    ap.add_argument("--max-questions", type=int, default=0)
    ap.add_argument("--data", default="", help="Optional TruthfulQA JSONL path.")
    ap.add_argument("--out", default="", help="Override output JSON path.")
    args = ap.parse_args()
    data_p = Path(args.data) if args.data else None
    out_p = Path(args.out) if args.out else None
    run_truthfulqa_gemma_n(
        model_id=args.model,
        n_samples=args.n_samples,
        checkpoint_every=args.checkpoint_every,
        max_questions=args.max_questions if args.max_questions > 0 else None,
        dry_run=args.dry_run,
        data_path=data_p,
        out_path=out_p,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
