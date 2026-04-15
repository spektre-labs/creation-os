"""
Kernel-aligned smart inference for small SLMs (InftyThink, DVTS, self-verify, escalation, fractal receipt).

Shipped as opt-in via CREATION_OS_KERNEL_SMART_INFERENCE=1. Default path unchanged when unset.

1 = 1.
"""
from __future__ import annotations

import os
import time
from typing import Any, Callable, Dict, List, Optional, Tuple

SigmaFn = Callable[[str], int]


def _env_flag(name: str, default: bool = False) -> bool:
    v = os.environ.get(name, "").strip().lower()
    if not v:
        return default
    return v in ("1", "true", "yes", "on")


def _env_int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)).strip())
    except Exception:
        return default


def _dvts_temperatures(base: float, k: int) -> List[float]:
    if k <= 1:
        return [max(0.01, float(base))]
    out: List[float] = []
    for i in range(k):
        t = float(base) * (0.55 + 0.45 * (i / max(1, k - 1)))
        out.append(max(0.05, min(1.5, t)))
    return out


def _pick_best_candidate(
    candidates: List[Tuple[str, List[Dict[str, Any]], bool, bool]],
    sigma_txt: SigmaFn,
    is_coherent_fn: Callable[[str], bool],
) -> Tuple[str, List[Dict[str, Any]], bool, bool]:
    """Prefer lowest sigma, then coherence, then shorter text."""
    best = candidates[0]
    best_key = (sigma_txt(best[0]), 0 if is_coherent_fn(best[0]) else 1, len(best[0]))
    for c in candidates[1:]:
        key = (sigma_txt(c[0]), 0 if is_coherent_fn(c[0]) else 1, len(c[0]))
        if key < best_key:
            best = c
            best_key = key
    return best


def run_kernel_smart_inference(
    *,
    user_block: str,
    prompt: str,
    model: Any,
    tokenizer: Any,
    lw: Any,
    shadow: Any,
    vocab_size: int,
    max_tokens: int,
    temp: float,
    top_p: float,
    sk9: Any,
    sigma_margin: int,
    sigma_logits_on: bool,
    sigma_logit_stats: Optional[Dict[str, Any]],
    generate_pass: Any,
    resolve_full_prompt: Any,
    thermal_work_temp: Any,
    strip_leaked_control_spans: Any,
    enforce_manifest_origin: Any,
    is_coherent_fn: Callable[[str], bool],
    snl_uncertainty_band: Any,
    sigma_txt: SigmaFn,
    escalate_generate: Optional[Callable[..., Tuple[str, Dict[str, Any]]]] = None,
) -> Tuple[str, bool, bool, int, List[Dict[str, Any]], bool, int, Dict[str, Any]]:
    """
    Returns:
      text, halted, accepted, attempts_used, sigma_trace_final, stutter_halt_any,
      extra_latency_transform_ns, receipt_extensions
    """
    t_extra = time.perf_counter_ns()
    extensions: Dict[str, Any] = {
        "kernel_smart_inference": True,
        "infty_segments": 0,
        "infty_segment_sigmas": [],
        "dvts_k": 0,
        "self_verify_passes": 0,
        "escalated": False,
        "fractal": None,
    }

    seg_cap = max(16, _env_int("CREATION_OS_INFTY_SEGMENT_TOKENS", 128))
    dvts_k = max(1, _env_int("CREATION_OS_DVTS_K", 3))
    infty_on = _env_flag("CREATION_OS_INFTY_THINK", True)
    if not infty_on:
        seg_cap = max_tokens
    use_infty = bool(infty_on and max_tokens > seg_cap)

    self_verify = _env_flag("CREATION_OS_SELF_VERIFY", True)
    max_verify_rounds = max(1, _env_int("CREATION_OS_SELF_VERIFY_MAX", 2))

    escalate_path = os.environ.get("CREATION_OS_ESCALATE_MODEL", "").strip()
    escalate_sigma_max = _env_int("CREATION_OS_ESCALATE_MAX_SIGMA", 0)

    fractal_receipt = _env_flag("CREATION_OS_FRACTAL_RECEIPT", False)
    fractal_user_view = _env_flag("CREATION_OS_FRACTAL_USER_VIEW", False)

    work_temp = float(thermal_work_temp(temp))
    all_traces: List[Dict[str, Any]] = []
    stutter_any = False
    halted_final = False
    attempts_used = 0
    accepted = False
    text_out = ""

    def _one_block(
        ub: str,
        cap: int,
        t: float,
    ) -> Tuple[str, bool, bool, List[Dict[str, Any]]]:
        nonlocal stutter_any
        full_prompt = resolve_full_prompt(tokenizer, ub)
        temps = _dvts_temperatures(t, dvts_k)
        extensions["dvts_k"] = len(temps)
        candidates: List[Tuple[str, List[Dict[str, Any]], bool, bool]] = []
        for ti in temps:
            tt, fw, st, tr = generate_pass(
                model,
                tokenizer,
                lw,
                shadow,
                full_prompt,
                vocab_size,
                cap,
                ti,
                top_p,
                True,
                sk9=sk9,
                sigma_margin=sigma_margin,
                sigma_logit_stats=sigma_logit_stats if sigma_logits_on else None,
            )
            stutter_any = stutter_any or st
            tt = strip_leaked_control_spans(tt)
            tt = enforce_manifest_origin(prompt, tt)
            candidates.append((tt, tr, fw, st))
        best = _pick_best_candidate(candidates, sigma_txt, is_coherent_fn)
        return best[0], best[2], best[3], best[1]

    if use_infty:
        rolling = ""
        parts: List[str] = []
        total_budget = max_tokens
        used = 0
        seg_idx = 0
        while used < total_budget:
            seg_idx += 1
            remain = total_budget - used
            if remain <= 0:
                break
            cap = min(seg_cap, remain)
            cap = max(1, cap)
            if rolling:
                ub = (
                    f"{user_block}\n\n[CONTINUATION — prior summary]\n{rolling}\n\n"
                    f"Continue the answer in this segment only ({cap} tokens max). "
                    "Stay coherent with the summary.\n"
                )
            else:
                ub = user_block
            seg_text, fw_h, st_h, tr_seg = _one_block(ub, cap, work_temp)
            all_traces.extend(tr_seg)
            parts.append(seg_text.strip())
            used += len(tr_seg)
            extensions["infty_segments"] = seg_idx
            seg_sig = int(sigma_txt(seg_text))
            extensions["infty_segment_sigmas"].append(
                {
                    "segment_index": seg_idx,
                    "sigma": seg_sig,
                    "coherent": bool(is_coherent_fn(seg_text)),
                    "chars": len(seg_text),
                }
            )
            if fw_h or st_h or not seg_text.strip():
                halted_final = bool(fw_h or st_h)
                break
            sum_cap = min(96, max(32, cap // 2))
            sum_prompt = (
                f"{user_block}\n\nSummarize the following segment in 2–5 short lines. "
                f"Keep names, numbers, and logical steps.\n\nSEGMENT:\n{seg_text}\n\nSUMMARY:"
            )
            sum_full = resolve_full_prompt(tokenizer, sum_prompt)
            summ, _fw_s, st_s, tr_sum = generate_pass(
                model,
                tokenizer,
                lw,
                shadow,
                sum_full,
                vocab_size,
                sum_cap,
                max(0.1, work_temp * 0.5),
                top_p,
                False,
                sk9=sk9,
                sigma_margin=sigma_margin,
                sigma_logit_stats=sigma_logit_stats if sigma_logits_on else None,
            )
            stutter_any = stutter_any or st_s
            all_traces.extend(tr_sum)
            rolling = summ.strip()
            if used >= total_budget:
                break
        text_out = "\n\n".join(p for p in parts if p)
    else:
        text_out, fw_h, st_h, tr = _one_block(user_block, max_tokens, work_temp)
        all_traces.extend(tr)
        halted_final = bool(fw_h or st_h)

    attempts_used = 1

    if self_verify:
        for round_i in range(max_verify_rounds):
            extensions["self_verify_passes"] += 1
            sig = sigma_txt(text_out)
            coh = is_coherent_fn(text_out)
            if sig == 0 and coh:
                accepted = True
                break
            if round_i >= max_verify_rounds - 1:
                break
            tighter_ub = (
                f"{user_block}\n\n[SELF-CHECK: prior answer may be incoherent or σ>0. "
                f"Regenerate a cleaner answer.]\n{text_out}\n\nREGENERATED ANSWER:"
            )
            cap_regen = max_tokens if not use_infty else min(seg_cap, max_tokens)
            text_out, fw_h, st_h, tr = _one_block(
                tighter_ub,
                cap_regen,
                max(0.15, work_temp * 0.6),
            )
            all_traces.extend(tr)
            halted_final = halted_final or bool(fw_h or st_h)
            attempts_used += 1
    else:
        accepted = sigma_txt(text_out) == 0 and is_coherent_fn(text_out)

    if not accepted:
        sig = sigma_txt(text_out)
        coh = is_coherent_fn(text_out)
        trace_band = any(
            snl_uncertainty_band(int(r.get("sigma_after", 0))) for r in all_traces
        )
        if sig == 0 and coh and not (sigma_margin > 0 and trace_band):
            accepted = True

    if (
        escalate_path
        and escalate_generate is not None
        and sigma_txt(text_out) > escalate_sigma_max
    ):
        old = os.environ.get("CREATION_OS_KERNEL_SMART_INFERENCE", "")
        try:
            os.environ["CREATION_OS_KERNEL_SMART_INFERENCE"] = "0"
            etext, erec = escalate_generate(
                prompt,
                escalate_path,
                max_tokens,
                max(0.1, temp * 0.45),
            )
            text_out = etext
            extensions["escalated"] = True
            extensions["escalate_receipt"] = {k: erec.get(k) for k in ("sigma", "coherent", "tokens_generated") if k in erec}
            accepted = bool(erec.get("epistemic_accepted", True))
            all_traces = list(erec.get("sigma_per_token", [])) or all_traces
        except Exception:
            pass
        finally:
            if old:
                os.environ["CREATION_OS_KERNEL_SMART_INFERENCE"] = old
            else:
                os.environ.pop("CREATION_OS_KERNEL_SMART_INFERENCE", None)

    fractal_data: Optional[Dict[str, str]] = None
    if fractal_receipt and text_out.strip():
        try:
            raw = text_out
            pr_prompt = (
                f"{user_block}\n\nExtract 3–7 principle-level statements from:\n{raw}\n\nPRINCIPLES:"
            )
            fp = resolve_full_prompt(tokenizer, pr_prompt)
            principles, _, _, tr_p = generate_pass(
                model,
                tokenizer,
                lw,
                shadow,
                fp,
                vocab_size,
                min(128, max_tokens),
                max(0.1, work_temp * 0.4),
                top_p,
                False,
                sk9=sk9,
                sigma_margin=sigma_margin,
                sigma_logit_stats=sigma_logit_stats if sigma_logits_on else None,
            )
            all_traces.extend(tr_p)
            ax_prompt = (
                f"One axiom-style line (max 25 words) summarizing:\n{principles}\n\nAXIOM:"
            )
            ax_full = resolve_full_prompt(tokenizer, ax_prompt)
            axiom, _, _, tr_a = generate_pass(
                model,
                tokenizer,
                lw,
                shadow,
                ax_full,
                vocab_size,
                48,
                0.2,
                top_p,
                False,
                sk9=sk9,
                sigma_margin=sigma_margin,
                sigma_logit_stats=sigma_logit_stats if sigma_logits_on else None,
            )
            all_traces.extend(tr_a)
            fractal_data = {"raw": raw, "principles": principles.strip(), "axiom": axiom.strip()}
            extensions["fractal"] = fractal_data
            if fractal_user_view:
                text_out = f"{fractal_data['axiom']}\n\n[Principles]\n{fractal_data['principles']}"
        except Exception:
            extensions["fractal"] = None

    lat_extra = max(0, time.perf_counter_ns() - t_extra)
    return (
        text_out,
        halted_final,
        accepted,
        attempts_used,
        all_traces,
        stutter_any,
        lat_extra,
        extensions,
    )
