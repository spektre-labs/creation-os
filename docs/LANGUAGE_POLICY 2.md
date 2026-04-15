# Language policy — Creation OS repository

**Rule:** The **only** language for **committed** material in this tree is **English**.

This document is the single reference for humans, agents, and CI expectations. Other project docs (CONTRIBUTING, AGENTS) point here.

---

## 1. What must be English

| Artifact | Requirement |
|----------|-------------|
| `README.md`, `ROADMAP.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `AGENTS.md`, and all of `docs/**/*.md` | **English only** — clear technical prose or lists. |
| **New** inline comments in C/C++ that explain behavior to maintainers | **English only** (match existing headers). |
| User-visible strings in demos where they teach the architecture | **English** (existing `printf` narrative in `creation_os_v2.c`). |
| Commit messages **on this product** when they describe merged repo state | **English** (project convention for `creation-os`). |

---

## 2. What may contain non-English text

| Artifact | Rule |
|----------|------|
| **Quoted** paper titles, names, or citations inside English prose | Allowed when attributed (bibliography, “as titled in …”). |
| **Oracle demo corpus** lines in `creation_os_v2.c` | Legacy demo strings; do **not** add new non-English demo text — extend in English. |
| **Issues and PR comments** on GitHub | Conversational language is your choice; anything **merged into** this tree must still satisfy §1. |

---

## 3. Pull requests

- Documentation patches must be **English** before merge.
- If a contributor writes another language, maintainers or agents **translate** and replace — do not merge bilingual primary prose.

---

## 4. Agents and automation

Coding agents (Cursor, Copilot, etc.) must **not** add Finnish, Japanese, emoji-only specification blocks, or mixed-language technical sections to tracked files. See also [AGENTS.md](../AGENTS.md).

---

## 5. Why this exists

One canonical language keeps:

- Cross-examination and review **fast** (no split-brain docs).  
- Search and grep **predictable**.  
- Academic citation and export **consistent**.

---

*Spektre Labs · Creation OS · 2026*
