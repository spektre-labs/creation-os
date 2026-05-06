# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Local model + σ-gate chat.

Start model first::

    llama-server -hf unsloth/Qwen3.6-35B-A3B-GGUF:UD-Q4_K_XL --port 8001 \\
      --chat-template-kwargs '{"preserve_thinking": true}'

Then::

    pip install 'creation-os[chat]'
    python examples/07_chat_local.py
"""
from __future__ import annotations

from cos.chat import SigmaChat

chat = SigmaChat(endpoint="http://localhost:8001/v1")
print("σ-chat (quit to exit)\n")
while True:
    user = input("> ")
    if user.strip().lower() in ("quit", "exit"):
        break
    result = chat.send(user)
    if result["error"]:
        print(f"Error: {result['error']}")
        continue
    print(f"[σ={result['sigma']:.3f} {result['verdict']}]")
    print(result["text"])

print(f"\nSession avg σ: {chat.session_sigma():.3f}")
