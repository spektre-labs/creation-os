# Canonical Git repository (read first)

## Single push target for this portable kernel

The **only** GitHub repository that receives **Creation OS kernel / CI / portable-tree** pushes from this product workflow is:

**https://github.com/spektre-labs/creation-os** (`main` branch)

---

## What stays elsewhere (no kernel push)

| Remote | Purpose |
|--------|---------|
| **spektre-labs/spektre-protocol** | Protocol + formal archive (Markdown). Push only when editing that archive. |
| **spektre-labs/corpus** | Papers and bibliographic material. Push only when editing corpus. |
| **lauri-elias-rainio/creation-os** | Author Genesis substrate (polyglot). Separate product surface; not the target for `creation_os_v*.c` landings. |

Full role map: **[REPOS_AND_ROLES.md](REPOS_AND_ROLES.md)**.

---

## Forbidden mistakes

- Pushing this **creation-os** directory to **protocol** or **corpus** as if it were the kernel home.
- Treating a **parent monorepo** root as `origin` for Creation OS when the intent is to update **spektre-labs/creation-os**.

---

## Correct workflow

| Goal | Action |
|------|--------|
| Ship kernel + docs + CI | Clone **spektre-labs/creation-os** (or rsync from publish script); **`git push`** to **`main`** only. |
| Edit protocol canon | Work in **spektre-protocol** clone; push **that** repo. |
| Edit papers / DOI index | Work in **corpus** clone; push **that** repo. |

---

*Spektre Labs · Creation OS · 2026*
