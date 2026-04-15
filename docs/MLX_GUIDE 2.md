# MLX (out of tree)

This repository is C-first. MLX integration lives in the wider Spektre workspace (`mlx_creation_os/`). If you combine them:

- Keep logits buffers stable across the native boundary.
- Prefer measured `make bench` numbers when comparing to GEMM baselines in documentation.
