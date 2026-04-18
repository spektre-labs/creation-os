# v142 — σ-Interop

> Creation OS works standalone. v142 makes it a **component** every
> major Python agentic framework can consume.

## Problem

Every real LLM deployment lives downstream of some orchestration
layer — LangChain, LlamaIndex, AutoGen, CrewAI, or the OpenAI SDK
itself. Creation OS was API-compatible (`v106` speaks OpenAI), but
the integration surface was missing: no `pip install`, no `COS(...)`
class, no LangChain `BaseChatModel`. v142 ships all of those with
**zero hard dependencies**.

## σ-innovation

Every chat response from Creation OS carries σ-metadata in
`X-COS-*` headers. v142's job is to **surface that metadata** as
first-class response fields so downstream agents can gate on σ,
route by specialist, or display confidence bands — without parsing
HTTP headers by hand.

## Contract

| Surface                                  | Status in v142.0 | Notes                                              |
| ---------------------------------------- | ----------------- | -------------------------------------------------- |
| `creation_os.COS`                        | shipped           | stdlib-only chat client                            |
| `creation_os.ChatResponse`               | shipped           | `.text` `.role` `.sigma_product` `.specialist` `.emitted` |
| `creation_os.openai_compat`              | shipped           | canonical v106 fixture + optional drop-in probe    |
| `creation_os.langchain_adapter`          | shipped           | lazy-imports `langchain_core`                      |
| `creation_os.llamaindex_adapter`         | shipped           | lazy-imports `llama_index.core`                    |
| `pyproject.toml` (pip install)           | shipped           | optional extras `[langchain]` `[llamaindex]` `[openai]` `[all]` |
| CrewAI / AutoGen agents                  | v142.1            | separate classes                                    |
| Streaming SSE client                     | v142.1            | server-sent-events for token-by-token responses    |
| HuggingFace-style `from_pretrained`      | v142.1            | auto-start v106 if no base_url given               |

## Example

```python
from creation_os import COS

cos = COS(base_url="http://localhost:8080")
r = cos.chat("Why is the sky blue?", sigma_threshold=0.5)
print(r.text)            # "Rayleigh scattering …"
print(r.sigma_product)   # 0.23
print(r.specialist)      # "bitnet-2b-reasoning"
print(r.emitted)         # True
```

LangChain:

```python
from creation_os.langchain_adapter import CreationOSLLM, LANGCHAIN_AVAILABLE
assert LANGCHAIN_AVAILABLE, "pip install langchain-core"
llm = CreationOSLLM(base_url="http://localhost:8080", sigma_threshold=0.5)
```

LlamaIndex (works even without the framework — falls back to stdlib):

```python
from creation_os.llamaindex_adapter import CreationOSQueryEngine
engine = CreationOSQueryEngine(llm_url="http://localhost:8080",
                                sigma_threshold=0.5)
response = engine.query("What is Creation OS?",
                        context=["Creation OS is a σ-gated kernel…"])
```

## Merge-gate measurements

No network required — every assertion runs offline.

| Stage                                            | Check                                                     |
| ------------------------------------------------ | --------------------------------------------------------- |
| 1. python3 availability                          | `python3 --version`                                        |
| 2. stdlib-only import smoke                      | `import creation_os` + canonical response parses correctly |
| 3. unittest suite                                | 8 tests — shape + header case + abstain + adapter imports  |
| 4. pyproject.toml sanity                         | name, version, optional-deps, tomllib parse (py ≥ 3.11)    |

## v142.0 / v142.1 split

**v142.0 (shipped)**

* `creation_os.COS` chat client.
* `ChatResponse` with σ-metadata.
* LangChain + LlamaIndex adapters (lazy-loaded).
* OpenAI SDK drop-in probe (graceful "not installed" return).
* `pip install creation-os` via `pyproject.toml` (stdlib-only core).

**v142.1 (follow-up)**

* CrewAI + AutoGen agent classes.
* SSE streaming client.
* `from_pretrained` that spins up v106 under the hood.
* Official PyPI publication.
* JavaScript/TypeScript SDK.

## Honest claims

* v142.0 **parses** v106's response shape — it does not run the
  kernel. End-to-end integration is verified by v106's own
  merge-gate, not by v142's.
* The SDK has no network test in CI. When a user points `COS` at a
  local v106, the existing v106 curl-loopback gate guarantees the
  shape; v142 proves the Python side of that contract.
* LangChain + LlamaIndex compatibility is *framework-version-bound*.
  The adapters target the stable APIs as of 2026-Q1 and may need a
  v142.1 refresh when those frameworks refactor.
