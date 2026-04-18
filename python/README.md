# `creation-os` — Python SDK for Creation OS

`pip install creation-os` — stdlib-only, σ-gated, OpenAI-compatible.

## Quick start

```python
from creation_os import COS

cos = COS(base_url="http://localhost:8080")
r = cos.chat("Hello", sigma_threshold=0.5)
print(r.text)            # "..."
print(r.sigma_product)   # 0.23
print(r.specialist)      # "bitnet-2b-reasoning"
print(r.emitted)         # True when the model did not abstain
```

## OpenAI SDK drop-in

```python
from openai import OpenAI
client = OpenAI(base_url="http://localhost:8080/v1", api_key="cos")
# The v106 kernel speaks the OpenAI Chat Completions shape; X-COS-*
# headers are already attached to every response.
```

## LangChain

```python
from creation_os.langchain_adapter import CreationOSLLM  # optional extra
llm = CreationOSLLM(base_url="http://localhost:8080", sigma_threshold=0.5)
```

## LlamaIndex

```python
from creation_os.llamaindex_adapter import CreationOSQueryEngine
engine = CreationOSQueryEngine(llm_url="http://localhost:8080", sigma_threshold=0.5)
ans = engine.query("Why is the sky blue?", context=["atmospheric scattering…"])
```

## Merge-gate scope (v142.0)

* `creation_os.COS` — chat client, σ-annotated responses.
* `creation_os.openai_compat` — OpenAI response-shape fixture + drop-in probe.
* `creation_os.langchain_adapter` — lazy-loaded, degrades if absent.
* `creation_os.llamaindex_adapter` — lazy-loaded, degrades if absent.

Hard deps: none. The SDK runs on Python ≥ 3.9 with only the stdlib.
