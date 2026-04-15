# Publish checklist — Creation OS (this repository only)

**Product remote:** `https://github.com/spektre-labs/creation-os.git`  
**Default branch:** `main`

All steps below assume **this directory is the Creation OS product** — no other repository name is required on your Mac for day-to-day work.

---

## 1. Before you push

Maintainer shortcut (clone + rsync + commit + push): see [MAINTAINERS.md](MAINTAINERS.md) (`make publish-github`).

```bash
make merge-gate   # portable check + v6..v26 self-tests (same as CI / publish script)
make bench        # recommended before citing throughput in README or talks
git status
```

For a minimal smoke test only (not sufficient before publish): `make check`.

Confirm `LICENSE`, `README.md`, and `docs/` read the way you want on GitHub’s first screen.

---

## 2a. One-step publish (recommended on your Mac)

From **this directory** (where `creation_os_v2.c` lives), with GitHub credentials working (`gh auth login` or SSH):

```bash
make publish-github
```

Optional environment variables:

| Variable | Default | Meaning |
|----------|---------|---------|
| `CREATION_OS_REMOTE` | `https://github.com/spektre-labs/creation-os.git` | Clone / push URL (use `git@github.com:spektre-labs/creation-os.git` for SSH) |
| `CREATION_OS_BRANCH` | `main` | Branch to update |
| `CREATION_OS_COMMIT_MSG` | `Publish portable Creation OS tree` | Commit message on the **creation-os** repo |

The script runs **`make merge-gate`**, clones **creation-os**, **rsync**s this tree (excluding `.git`), commits if there are changes, and **`git push`**.

---

## 2. Push when this folder is already the git root

`origin` → `https://github.com/spektre-labs/creation-os.git`:

```bash
git add -A
git commit -m "Your message"
git push origin main
```

Use HTTPS + PAT or SSH as you prefer for **creation-os** only.

---

## 3. Push when you keep a copy of this tree inside another local folder

Copy **only** this Creation OS tree into a **fresh clone** of **creation-os**, then commit and push from that clone. Replace `SRC` with the absolute path to **this** folder (the one containing `creation_os_v2.c` and `Makefile`):

```bash
STAGE=$(mktemp -d)
SRC="/ABSOLUTE/PATH/TO/creation-os"
git clone https://github.com/spektre-labs/creation-os.git "$STAGE/repo"
find "$STAGE/repo" -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +
rsync -a "$SRC/" "$STAGE/repo/"
cd "$STAGE/repo"
git add -A
git status
git commit -m "Sync Creation OS tree"
git push origin main
```

---

## 4. After push

- Confirm GitHub Actions (`.github/workflows/ci.yml`) is green.  
- Spot-check `README` images (`docs/assets/*.svg`).

---

*Spektre Labs · Creation OS · 2026*
