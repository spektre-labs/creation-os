# Maintainer notes — Creation OS (`creation-os`)

**Audience:** people with merge rights to **https://github.com/spektre-labs/creation-os** and authors preparing releases from a **local** copy of this tree.

---

## 1. Publishing to GitHub (`creation-os` only)

From the **root of this directory** (where `creation_os_v2.c` lives):

```bash
make check
make check-v6   # Living Kernel — run when v6 source or its tests/docs change
make publish-github
```

This clones `spektre-labs/creation-os`, `rsync`s this tree (excluding `.git`), commits if there are diffs, and pushes `main`. See [publish_checklist_creation_os.md](publish_checklist_creation_os.md) and environment variables `CREATION_OS_REMOTE`, `CREATION_OS_BRANCH`, `CREATION_OS_COMMIT_MSG` in that file.

**Do not** point `CREATION_OS_REMOTE` at unrelated product repositories by mistake.

---

## 2. Language and claims (merge gate)

- **English only** for all committed prose: [LANGUAGE_POLICY.md](LANGUAGE_POLICY.md).  
- **Benchmarks and comparisons:** [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md), [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md).  
- PR template checklist: [.github/PULL_REQUEST_TEMPLATE.md](../.github/PULL_REQUEST_TEMPLATE.md).

---

## 3. Security

User-facing policy: [SECURITY.md](../SECURITY.md). Developer-oriented abuse patterns: [SECURITY_DEVELOPER_NOTES.md](SECURITY_DEVELOPER_NOTES.md).

---

## 4. CI

Workflow: [.github/workflows/ci.yml](../.github/workflows/ci.yml) — `make check && make check-v6` on Ubuntu for pushes and PRs to `main` / `master`.

---

*Spektre Labs · Creation OS · 2026*
