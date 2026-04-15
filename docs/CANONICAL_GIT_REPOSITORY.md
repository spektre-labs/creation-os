# Canonical Git repository (read first)

## Single source of truth

The **only** product Git repository for this portable tree is:

**https://github.com/spektre-labs/creation-os** (`main` branch)

All releases, CI, and contributor `git clone` / `git push` for Creation OS must target **that** repository.

---

## What is forbidden

- **Do not** treat a parent monorepo (e.g. any “protocol” or umbrella workspace that happens to contain a `creation-os/` directory) as the canonical place to `git push` Creation OS changes.
- **Do not** run `git remote add origin https://github.com/spektre-labs/creation-os.git` (or SSH equivalent) on a repository whose root is **not** the Creation OS tree root. That misroutes pushes and corrupts contributor mental models.
- **Do not** merge unrelated histories from a parent repo into `spektre-labs/creation-os`.

---

## Correct workflows

| Goal | Do this |
|------|---------|
| Daily work | Work in a **clone whose root is** the Creation OS repo (this directory when checked out alone). |
| Publish from a nested copy | From **this** directory (where `creation_os_v2.c` exists), run `make publish-github` — see [publish_checklist_creation_os.md](publish_checklist_creation_os.md). That script clones `spektre-labs/creation-os` to a temp dir, rsyncs **this** tree, commits, and pushes. |
| CI | GitHub Actions on **spektre-labs/creation-os** only (see `.github/workflows/ci.yml` here). |

---

*Spektre Labs · Creation OS · 2026*
