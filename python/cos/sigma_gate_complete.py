"""
Three-stage **σ** pipeline scaffold: precheck (question-only) → optional streaming decode
halt → optional post gate (e.g. ``SigmaUltimate`` / ``SigmaGate``).

This is an integration harness; thresholds and probe bundles need calibration per model.
See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from typing import Any, Dict, Optional

from .sigma_gate_precheck import SigmaPrecheck
from .sigma_streaming import StreamingSigmaGate


class SigmaGateComplete:
    """
    Layer 1: ``SigmaPrecheck`` (no completion yet).
    Layer 2: optional ``StreamingSigmaGate.generate_with_gate``.
    Layer 3: optional post gate; must be callable ``(model, tok, prompt, response, **kw)``.
    """

    def __init__(
        self,
        *,
        precheck: Optional[SigmaPrecheck] = None,
        streaming: Optional[StreamingSigmaGate] = None,
        post_gate: Any = None,
    ):
        self.precheck = precheck or SigmaPrecheck(tau_skip=0.80, mode="entropy")
        self.streaming = streaming
        self.post_gate = post_gate

    def process(
        self,
        model: Any,
        tokenizer: Any,
        question: str,
        *,
        max_new_tokens: int = 200,
        reference: Optional[str] = None,
        max_new_tokens_post: Optional[int] = None,
    ) -> Dict[str, Any]:
        mnt_post = int(max_new_tokens_post) if max_new_tokens_post is not None else int(max_new_tokens)
        result: Dict[str, Any] = {
            "question": question,
            "layers_passed": [],
            "response": None,
            "decision": None,
            "tokens_generated": 0,
            "tokens_saved": 0,
        }

        should_gen, sigma_pre, pre_details = self.precheck.predict(model, tokenizer, question)
        result["sigma_pre"] = float(sigma_pre)
        result["pre_details"] = pre_details

        if not should_gen:
            result["decision"] = "ABSTAIN"
            result["reason"] = "pre_generation_risk"
            result["tokens_saved"] = int(max_new_tokens)
            return result

        result["layers_passed"].append("precheck")

        if self.streaming is not None:
            response, stream_meta = self.streaming.generate_with_gate(
                model,
                tokenizer,
                question,
                max_new_tokens=int(max_new_tokens),
            )
            result["response"] = response
            result["stream_meta"] = stream_meta
            result["tokens_generated"] = int(stream_meta.get("tokens_generated", 0))
            if stream_meta.get("halted"):
                result["decision"] = "ABSTAIN"
                result["reason"] = "streaming_halt"
                result["halt_detail"] = stream_meta.get("halt_reason")
                result["tokens_saved"] = int(max_new_tokens) - int(stream_meta.get("tokens_generated", 0))
                result["layers_passed"].append("streaming")
                return result
            result["layers_passed"].append("streaming")
        else:
            import torch

            if tokenizer.pad_token is None:
                tokenizer.pad_token = tokenizer.eos_token
            inputs = tokenizer(question, return_tensors="pt")
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
            result["response"] = response
            result["tokens_generated"] = int(out.shape[1] - inputs["input_ids"].shape[1])

        if self.post_gate is not None and result["response"]:
            try:
                post_ret = self.post_gate(
                    model,
                    tokenizer,
                    question,
                    result["response"],
                    reference=reference,
                    max_new_tokens=mnt_post,
                )
            except TypeError:
                post_ret = self.post_gate(
                    model,
                    tokenizer,
                    question,
                    result["response"],
                    reference=reference,
                )
            if isinstance(post_ret, tuple) and len(post_ret) >= 2:
                sigma_post, decision = float(post_ret[0]), str(post_ret[1])
                result["sigma_post"] = sigma_post
                result["post_details"] = post_ret[2] if len(post_ret) >= 3 else {}
                result["decision"] = decision
            result["layers_passed"].append("post_check")
        else:
            result["decision"] = "ACCEPT"

        if result.get("tokens_saved", 0) == 0 and result["decision"] != "ABSTAIN":
            result["tokens_saved"] = 0

        return result
