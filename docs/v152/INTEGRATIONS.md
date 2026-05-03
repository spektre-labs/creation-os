# v152 — Framework integrations (σ-gate)

Optional shims add a **single wiring point** per stack so each LLM turn can carry **σ** and a **verdict** (ACCEPT / RETHINK / ABSTAIN) without changing C headers or `sigma_gate.h`.

Install extras as needed:

```bash
pip install 'creation-os[langchain]'
pip install 'creation-os[crewai]'
pip install 'creation-os[autogen]'
# or all optional frameworks:
pip install 'creation-os[frameworks]'
```

## CLI

From the repo (with `PYTHONPATH=python` or a pip-installed `creation-os`):

```bash
./cos integrations --check
./cos integrations --langchain --example
./cos integrations --crewai --example
./cos integrations --autogen --example
```

## LangChain (callback)

Passive: scores each completion and appends to `callback.traces`. Default `on_abstain="warn"` emits a warning instead of raising.

```python
from cos.integrations.langchain_sigma import SigmaGateCallback

callback = SigmaGateCallback()  # optional gate: SigmaGateCallback(my_gate, on_abstain="raise")
result = chain.invoke(inp, config={"callbacks": [callback]})
```

See also `sigma_gated` and `SigmaCallback` (alias) in the same module. That
``sigma_gated`` factory wraps **plain str** callables and returns
``{"result", "sigma", "verdict"}``; for the parameterless decorator that
raises on ABSTAIN, use :func:`cos.decorators.sigma_gated`.

## LangGraph-style node

```python
from cos.integrations.langgraph_sigma import sigma_gate_node, sigma_gate_router

state2 = sigma_gate_node(state, gate=None, append_abstain_system_note=False)
```

## CrewAI (tool)

Active: the agent chooses when to call the tool.

```python
from cos.integrations.crewai_sigma import SigmaGateTool

tool = SigmaGateTool()  # optional: SigmaGateTool(gate=my_gate)
# Agent(..., tools=[tool], ...)
```

## AutoGen AgentChat–style hook

Middleware-style: mutates the last message dict (adds `sigma` / `verdict`, optional ABSTAIN suffix on `content`).

```python
from cos.integrations.autogen_sigma import SigmaAutoGenHook

hook = SigmaAutoGenHook()
# agent.register_hook("process_last_received_message", hook.process_last_received_message)
```

`SigmaGateHook` remains a compatibility alias; `SigmaGateHook.process_message` scores a single message copy with an empty prompt context.

## Scoring backend

With `gate=None`, modules use the same **quickstart** scorer as other lab entrypoints. Pass a trained `SigmaGate` (LSD-backed) when you need trajectory-probe scores; see `docs/CLAIM_DISCIPLINE.md` before mixing lab demos with harness receipts.
