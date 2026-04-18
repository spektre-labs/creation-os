# PyPI publish workflow (v155.1)

The canonical source of the Python SDK lives at
[`python/`](../../python/) — its `pyproject.toml` is what
`twine upload` consumes. This directory only holds the
human-readable release recipe so `make check-v155` has something
to lint offline.

## v155.0 — what is validated offline

`scripts/v155_publish_check.py` asserts:

- `python/pyproject.toml` parses as TOML and names
  `project.name = "creation-os"` and a non-empty version.
- `python/creation_os/__init__.py` exists and exports `COS`.
- Every README / model card / npm manifest in
  [`packaging/`](..) parses and is non-empty.

## v155.1 — release recipe

```bash
cd python
python -m build            # sdist + wheel
twine check dist/*
twine upload dist/*        # requires $PYPI_API_TOKEN
```

Pre-flight from repo root:

```bash
make check-v155            # offline manifest check
cd python && python -m pytest   # SDK unit tests
```

Post-release smoke:

```bash
pip install --upgrade creation-os
python -c "from creation_os import COS; print(COS)"
```
