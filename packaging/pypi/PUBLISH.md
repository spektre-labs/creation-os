# PyPI publish workflow (v155.1)

The canonical **`cos`** runtime + CLI is built from the repo-root
[`pyproject.toml`](../../pyproject.toml) (package `creation-os`).

The OpenAI-compatible **interop SDK** (`from creation_os import COS`) lives under
[`python/`](../../python/) — its [`python/pyproject.toml`](../../python/pyproject.toml)
publishes as **`creation-os-interop-sdk`** on PyPI when that split upload is used.
This directory only holds the human-readable release recipe so `make check-v155`
has something to lint offline.

## v155.0 — what is validated offline

`scripts/v155_publish_check.py` asserts:

- Repo root `pyproject.toml` parses as TOML and names
  `project.name = "creation-os"`, a non-empty version, and `cos` → `cos.cli:main`.
- `python/pyproject.toml` parses as TOML and names
  `project.name = "creation-os-interop-sdk"` and a non-empty version.
- `python/creation_os/__init__.py` exists and exports `COS`.
- Every README / model card / npm manifest in
  [`packaging/`](..) parses and is non-empty.

## v155.1 — release recipe

**Main `cos` package (PyPI `creation-os`):**

```bash
python -m build            # sdist + wheel from repo root
twine check dist/*
twine upload dist/*        # requires $PYPI_API_TOKEN
```

**Interop SDK (PyPI `creation-os-interop-sdk`):**

```bash
cd python
python -m build
twine check dist/*
twine upload dist/*
```

Pre-flight from repo root:

```bash
make check-v155            # offline manifest check
cd python && python -m pytest   # SDK unit tests
```

Post-release smoke:

```bash
pip install --upgrade creation-os
cos version
cos gate --prompt "What is 2+2?" --response "4"

pip install --upgrade creation-os-interop-sdk
python -c "from creation_os import COS; print(COS)"
```
