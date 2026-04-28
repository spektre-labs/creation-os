"""
Streaming / per-token **scaffold** for hidden-state hooks during generation.

Motivation: real-time entity or token-level probes (e.g. linear probes during decode).
This implementation records a cheap per-step scalar from the **last decoder layer**
last-token hidden state (L2 norm, mapped to [0, 1]) — **not** the full LSD σ probe,
which is trained on trajectory features and would require a separate token-aligned
head and calibration.

Use as a hook template; swap ``_scalar_from_hidden`` for your probe once dimensions
and training match your checkpoint.
"""
from __future__ import annotations

from typing import Any, Callable, List, Optional

import numpy as np


class StreamingSigmaScaffold:
    """Collect per-generated-token scalars via a forward hook (demo signal)."""

    def __init__(self, *, tau_interrupt: float = 0.99):
        self.tau_interrupt = float(tau_interrupt)
        self.token_sigmas: List[float] = []
        self.should_stop = False
        self._hook_handle = None

    def reset(self) -> None:
        self.token_sigmas.clear()
        self.should_stop = False

    def _scalar_from_hidden(self, h_last: "Any") -> float:
        import torch

        if not isinstance(h_last, torch.Tensor):
            return 0.5
        v = h_last.detach().float().norm().item()
        return float(np.clip(v / 200.0, 0.0, 1.0))

    def _hook_fn(self, module: Any, args: Any, output: Any) -> None:
        import torch

        hidden = output[0] if isinstance(output, tuple) else getattr(output, "last_hidden_state", output)
        if not isinstance(hidden, torch.Tensor) or hidden.ndim < 3:
            return
        last = hidden[:, -1, :]
        sigma = self._scalar_from_hidden(last)
        self.token_sigmas.append(sigma)
        if sigma > self.tau_interrupt:
            self.should_stop = True

    def register(self, model: Any, *, layer_selector: Optional[Callable[[Any], Any]] = None) -> None:
        """Attach hook to last transformer block by default (GPT-2 style ``h``)."""
        if layer_selector is not None:
            target = layer_selector(model)
        else:
            target = getattr(model, "transformer", model)
            target = getattr(target, "h", None)
            if target is not None and len(target) > 0:
                target = target[-1]
            else:
                target = model
        if self._hook_handle is not None:
            self._hook_handle.remove()
        self._hook_handle = target.register_forward_hook(self._hook_fn)

    def unregister(self) -> None:
        if self._hook_handle is not None:
            self._hook_handle.remove()
            self._hook_handle = None

    def get_sigma_trajectory(self) -> List[float]:
        return list(self.token_sigmas)


# Back-compat alias for docs / external snippets that use this name.
StreamingSigmaGate = StreamingSigmaScaffold
