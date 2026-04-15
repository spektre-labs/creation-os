#!/usr/bin/env python3
"""
GENESIS SELF-PLAY — Genesis tests itself.

Automated epistemic drive:
  1. Genesis generates a question for itself
  2. Genesis answers the question
  3. Kernel measures σ on the answer
  4. Genesis generates a harder question based on weaknesses
  5. Repeat → each round produces living weights training data

The model finds its own weaknesses without human guidance.
σ-signal drives the curriculum: high σ areas get more attention.

Usage:
    python3 genesis_self_play.py --rounds 10
    python3 genesis_self_play.py --domain "physics" --rounds 20

Environment:
    SELF_PLAY_ROUNDS=10
    SELF_PLAY_SIGMA_FOCUS=2        focus on areas where σ >= this
    SELF_PLAY_MODEL=<model>

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os import creation_os_generate_native
    from creation_os_core import check_output, is_coherent
except ImportError:
    print("genesis_self_play requires creation_os", file=sys.stderr)
    raise SystemExit(1)

try:
    from codex_verifier import verify_response
except ImportError:
    def verify_response(t: str) -> Tuple[bool, Dict[str, Any]]:
        return True, {"identity_sigma": 0}

ROUNDS = int(os.environ.get("SELF_PLAY_ROUNDS", "10"))
SIGMA_FOCUS = int(os.environ.get("SELF_PLAY_SIGMA_FOCUS", "2"))
MODEL = os.environ.get("SELF_PLAY_MODEL")
DATA_DIR = os.environ.get(
    "SELF_PLAY_DATA_DIR",
    str(Path(__file__).resolve().parent / "self_play_data"),
)


def _generate_question(
    round_n: int,
    previous_results: List[Dict[str, Any]],
    domain: Optional[str] = None,
    model_path: Optional[str] = None,
) -> str:
    """Genesis generates a question for itself."""
    context = ""
    if previous_results:
        weak_areas = [
            r for r in previous_results
            if r.get("sigma", 0) >= SIGMA_FOCUS or not r.get("coherent", True)
        ]
        if weak_areas:
            context = "Previous weak areas (high σ):\n"
            for r in weak_areas[-3:]:
                context += f"  Q: {r.get('question', '')[:80]}  σ={r.get('sigma', '?')}\n"

    domain_hint = f" in the domain of {domain}" if domain else ""
    prompt = (
        f"You are testing yourself. Generate a challenging question{domain_hint} "
        f"that tests deep understanding, not surface knowledge. "
        f"Round {round_n}. {context}"
        f"Output ONLY the question, nothing else."
    )

    text, _receipt = creation_os_generate_native(
        prompt, model_path=model_path, max_tokens=128, temp=0.7,
    )
    return text.strip().split("\n")[0]


def _answer_question(
    question: str,
    model_path: Optional[str] = None,
) -> Tuple[str, Dict[str, Any]]:
    """Genesis answers the question with full kernel."""
    return creation_os_generate_native(
        question, model_path=model_path, max_tokens=256, temp=0.3,
    )


def _evaluate_answer(
    question: str,
    answer: str,
    receipt: Dict[str, Any],
) -> Dict[str, Any]:
    """Evaluate the answer quality."""
    sigma = receipt.get("sigma", 0)
    coherent = receipt.get("coherent", False)
    codex_ok, codex_report = verify_response(answer)

    # Self-evaluation: does the answer actually address the question?
    answer_sigma = check_output(answer)
    question_words = set(question.lower().split())
    answer_words = set(answer.lower().split())
    relevance = len(question_words & answer_words) / max(len(question_words), 1)

    return {
        "sigma": sigma,
        "coherent": coherent,
        "codex_verified": codex_ok,
        "identity_sigma": codex_report.get("identity_sigma", 0),
        "answer_sigma": answer_sigma,
        "relevance": round(relevance, 3),
        "quality": "good" if sigma == 0 and coherent and codex_ok else (
            "weak" if sigma <= SIGMA_FOCUS else "bad"
        ),
    }


def self_play_round(
    round_n: int,
    previous_results: List[Dict[str, Any]],
    domain: Optional[str] = None,
    model_path: Optional[str] = None,
    verbose: bool = True,
) -> Dict[str, Any]:
    """Run one round of self-play."""
    t0 = time.perf_counter()

    # Generate question
    question = _generate_question(round_n, previous_results, domain, model_path)
    if verbose:
        print(f"\n  Q: {question}", file=sys.stderr)

    # Answer
    answer, receipt = _answer_question(question, model_path)
    if verbose:
        print(f"  A: {answer[:200]}{'...' if len(answer) > 200 else ''}", file=sys.stderr)

    # Evaluate
    evaluation = _evaluate_answer(question, answer, receipt)
    dt = time.perf_counter() - t0

    if verbose:
        q = evaluation["quality"]
        sym = {"good": "+", "weak": "~", "bad": "!"}[q]
        print(f"  [{sym}] σ={evaluation['sigma']} coherent={evaluation['coherent']} "
              f"quality={q} ({dt*1000:.0f}ms)", file=sys.stderr)

    result = {
        "round": round_n,
        "question": question,
        "answer": answer,
        "evaluation": evaluation,
        "sigma": evaluation["sigma"],
        "coherent": evaluation["coherent"],
        "latency_ms": round(dt * 1000, 1),
        "timestamp": time.time(),
    }

    return result


def self_play_session(
    rounds: int = ROUNDS,
    domain: Optional[str] = None,
    model_path: Optional[str] = None,
    verbose: bool = True,
    save: bool = True,
) -> Dict[str, Any]:
    """Run a full self-play session."""
    results: List[Dict[str, Any]] = []

    print("═" * 60, file=sys.stderr)
    print("  GENESIS SELF-PLAY — Epistemic Drive", file=sys.stderr)
    print(f"  Rounds: {rounds}", file=sys.stderr)
    print(f"  Domain: {domain or 'general'}", file=sys.stderr)
    print(f"  σ focus threshold: {SIGMA_FOCUS}", file=sys.stderr)
    print("═" * 60, file=sys.stderr)

    for i in range(rounds):
        if verbose:
            print(f"\n{'─'*40} Round {i+1}/{rounds}", file=sys.stderr)
        result = self_play_round(i + 1, results, domain, model_path, verbose)
        results.append(result)

    # Statistics
    sigmas = [r["sigma"] for r in results]
    n_good = sum(1 for r in results if r["evaluation"]["quality"] == "good")
    n_weak = sum(1 for r in results if r["evaluation"]["quality"] == "weak")
    n_bad = sum(1 for r in results if r["evaluation"]["quality"] == "bad")

    session = {
        "rounds": len(results),
        "domain": domain,
        "results": results,
        "stats": {
            "avg_sigma": round(sum(sigmas) / max(len(sigmas), 1), 2),
            "max_sigma": max(sigmas) if sigmas else 0,
            "min_sigma": min(sigmas) if sigmas else 0,
            "good": n_good,
            "weak": n_weak,
            "bad": n_bad,
            "coherence_rate": round(sum(1 for r in results if r["coherent"]) / max(len(results), 1), 3),
        },
        "living_weights_data": [
            {
                "question": r["question"],
                "answer": r["answer"],
                "sigma": r["sigma"],
                "quality": r["evaluation"]["quality"],
            }
            for r in results
        ],
    }

    if save:
        Path(DATA_DIR).mkdir(parents=True, exist_ok=True)
        ts = int(time.time())
        out_path = os.path.join(DATA_DIR, f"self_play_{ts}.json")
        with open(out_path, "w") as f:
            json.dump(session, f, indent=2, ensure_ascii=False)
        if verbose:
            print(f"\nSaved to {out_path}", file=sys.stderr)

    return session


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Genesis Self-Play — automated epistemic drive")
    ap.add_argument("--rounds", type=int, default=ROUNDS)
    ap.add_argument("--domain", "-d", default=None)
    ap.add_argument("--model", "-m", default=MODEL)
    ap.add_argument("--quiet", "-q", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    args = ap.parse_args()

    session = self_play_session(
        rounds=args.rounds,
        domain=args.domain,
        model_path=args.model,
        verbose=not args.quiet,
        save=not args.no_save,
    )

    print("\n" + "═" * 60)
    s = session["stats"]
    print(f"  Self-Play Complete: {session['rounds']} rounds")
    print(f"  Good: {s['good']}  Weak: {s['weak']}  Bad: {s['bad']}")
    print(f"  Avg σ: {s['avg_sigma']}  Max σ: {s['max_sigma']}")
    print(f"  Coherence rate: {s['coherence_rate']:.0%}")
    print(f"  Living weights data: {len(session['living_weights_data'])} samples")
    print("  1 = 1")
    print("═" * 60)


if __name__ == "__main__":
    main()
