"""
Full sigma pipeline: optional precheck (question-only) + generation + LSD post-gate.

- ``sigma_post`` / decision come from ``SigmaGate`` (LSD pickle, same as v4).
- ``sigma_pre`` comes from ``SigmaPrecheck`` (entropy heuristic or optional head).

This is an integration scaffold; thresholds need calibration on your traffic.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, Optional

from .sigma_gate import SigmaGate
from .sigma_gate_precheck import SigmaPrecheck


class SigmaGateFull:
    def __init__(
        self,
        probe_path: str,
        *,
        precheck_mode: str = "entropy",
        precheck_head_path: Optional[str] = None,
        tau_accept: float = 0.3,
        tau_abstain: float = 0.7,
        tau_skip: float = 0.85,
    ):
        self.post_gate = SigmaGate(probe_path, tau_accept=tau_accept, tau_abstain=tau_abstain)
        self.pre_gate = SigmaPrecheck(
            tau_skip=tau_skip,
            mode=precheck_mode,
            head_bundle_path=precheck_head_path,
        )
        self.precheck_mode = precheck_mode

    def close(self) -> None:
        self.post_gate.close()

    def __enter__(self) -> SigmaGateFull:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def __call__(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        generate_fn: Optional[Callable[[str], str]] = None,
        *,
        max_new_tokens: int = 200,
    ) -> Dict[str, Any]:
        sigma_pre: Optional[float] = None

        should_gen, sigma_pre = self.pre_gate.should_generate(model, tokenizer, prompt)
        if not should_gen:
            return {
                "sigma_pre": float(sigma_pre),
                "sigma_post": None,
                "sigma": float(sigma_pre),
                "decision": "ABSTAIN",
                "reason": "pre_generation_uncertainty",
                "response": "",
                "tokens_saved": True,
                "precheck_mode": self.precheck_mode,
            }

        if generate_fn is not None:
            response = generate_fn(prompt)
        else:
            import torch

            if tokenizer.pad_token is None:
                tokenizer.pad_token = tokenizer.eos_token
            inputs = tokenizer(prompt, return_tensors="pt")
            dev = next(model.parameters()).device
            inputs = {k: v.to(dev) for k, v in inputs.items()}
            with torch.no_grad():
                out = model.generate(
                    **inputs,
                    max_new_tokens=int(max_new_tokens),
                    do_sample=False,
                    pad_token_id=tokenizer.pad_token_id,
                )
            response = tokenizer.decode(
                out[0][inputs["input_ids"].shape[1] :],
                skip_special_tokens=True,
            ).strip()

        sigma_post, decision = self.post_gate(model, tokenizer, prompt, response)
        w_pre, w_post = 0.25, 0.75
        sigma_combined = float(w_pre * float(sigma_pre) + w_post * float(sigma_post))

        return {
            "sigma_pre": float(sigma_pre),
            "sigma_post": float(sigma_post),
            "sigma": float(sigma_post),
            "sigma_combined": sigma_combined,
            "decision": decision,
            "response": response,
            "tokens_saved": False,
            "precheck_mode": self.precheck_mode,
        }
