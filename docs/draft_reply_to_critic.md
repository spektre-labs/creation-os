# Draft reply to critic (not auto-posted)

This is a **draft** reply. Please edit to your voice before posting anywhere.

---

Thanks for the critique. You’re right to push on **what is actually measured in-repo** versus what was implied in posts.

## 1) v2 vs v26 — what I should have made explicit

The biggest confusion is file selection:

- **`creation_os_v2.c`** is a **single-file bootstrap demo** (teaching spine). It’s meant to communicate the BSC primitives and the “26 modules” narrative, not to act as a merge-gate harness or benchmark artifact.
- **`creation_os_v26.c`** is the **flagship merge-gate harness**. Running `./creation_os_v26 --self-test` prints **184/184 PASS** when the repo is in a coherent state.

I failed by letting public discussion blur those tiers. I’ve now added a reviewer map that says this plainly: **`docs/WHICH_FILE_TO_READ.md`**, and the README now points to it immediately.

## 2) Claim tiers — measured vs theoretical vs projected vs external

I’ve tightened the evidence discipline in a single place: **`docs/WHAT_IS_REAL.md`**. Every public-facing “headline style” claim now has a tier tag:

- **M**: measured in this repo (command exists, green on supported hosts)
- **T**: theoretical/derived (not measured here)
- **P**: projected (not measured here)
- **E**: external evidence (not in this repo; must link to an archive)
- **R**: retired/corrected (prior phrasing was wrong or ungrounded)

Concrete corrections:

- “**87,000×**” is now explicitly treated as **T (theoretical)** unless backed by an archived repro bundle and a harness row.
- “**5.8 W**” is explicitly **P (projected)** unless measured instrumentation + archive exists.
- “**1,052 cases**” is treated as **E (external)** — it is not stored in this repo tree.

I also added a **Retired claims** section acknowledging where earlier phrasing crossed the line into “sounds measured” when it wasn’t.

## 3) Reviewer flow — one command

To make this reviewable without guesswork, there is now:

```bash
make reviewer
```

That target runs:

- portable checks (`make check`)
- the flagship harness (`make check-v26`)
- v2’s `--self-test`
- and a discipline check that `docs/WHAT_IS_REAL.md` remains tier-tagged.

If `make reviewer` is green, the repo’s public claims have an explicit evidence map and the basic harness surface is consistent.

## 4) GDA duplication and exploratory folders

There were duplicate / confusing GDA fragments. I added a conservative archival tool that only moves **byte-identical duplicates** into `gda/_archive/` and leaves `gda/` canonical. The archive is explicitly labeled **exploratory, not load-bearing**.

## 5) Tone: what I’m committing to

You were right that the only responsible way forward is:

- no more “headline numbers” without a tier tag and an artifact you can run
- no more ambiguous “this repo proves X” phrasing
- no more posting until the next demo/harness is actually coherent end-to-end

If you spot any remaining public claim that lacks a tier tag or a reproducible command, point it out — that’s a real bug.

