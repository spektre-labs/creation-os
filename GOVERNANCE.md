# Creation OS — Governance

**TL;DR.** Creation OS is led by a benevolent dictator (BDFL). All
code merges require passing the merge-gate. All commercial rights
sit with Lauri Rainio and Spektre Labs Oy, jointly and severally,
as set out in the [`LICENSE`](LICENSE) dispatcher.

## Roles

| Role | Who | Scope |
|---|---|---|
| **BDFL** | Lauri Elias Rainio | Final call on kernel scope, σ-contracts, merge-gate thresholds, release cadence, and license terms. |
| **Maintainers** | Spektre Labs Oy core engineers | Review and approve pull requests; enforce the merge-gate; triage issues. |
| **Contributors** | Anyone who signs the [`CLA.md`](CLA.md) | May open PRs, file issues, submit benchmarks, propose new kernels. |

## The 1 = 1 invariant

Every pull request MUST satisfy the `1 = 1` invariant:

> What is declared in the README, in the kernel docstring, and in
> the merge-gate output must equal what is realized by the kernel
> at runtime.

If a PR adds text to the README ("kernel X achieves Y") without
also adding a merge-gate target that falsifies Y on every CI run,
the PR will be asked to add the gate or remove the text. No
aspirational claims in tracked prose.

## Merge requirements

A pull request is eligible for merge iff **every** item below is true:

1. `make merge-gate` exits 0 from a clean checkout of the PR.
2. Every new kernel adds a `src/vN/`, a `benchmarks/vN/check_*.sh`,
   a `docs/vN/README.md`, and a `check-vN` Makefile target wired
   into `merge-gate`.
3. Every new benchmark number in the README is either
   - labelled **tier-0 synthetic** (deterministic fixture), or
   - backed by a reproducible script, *and* an archived trace, *and*
     the host metadata specified by
     [`docs/REPRO_BUNDLE_TEMPLATE.md`](docs/REPRO_BUNDLE_TEMPLATE.md).
4. `σ_code < 0.30` on any LLM-generated code (v151 σ-code-agent
   gate) before it enters the tree.
5. The author has signed the [Contributor License Agreement](CLA.md).
6. The [Language Policy](docs/LANGUAGE_POLICY.md) (English-only for
   tracked prose) is respected.
7. The PR does NOT delete or contradict existing **Limitations**,
   **Measured results** footnotes, or **Publication-hard**
   commitments without a maintainer's explicit approval.

## Decision-making

* **Normal changes** (bug fixes, doc typos, kernel additions that
  follow an existing pattern) are merged by any maintainer after
  review + green gate.
* **Structural changes** (new σ-channel, new merge-gate target,
  license-affecting change, new public HTTP route) require BDFL
  sign-off. Open a PR labelled `rfc:` and tag @spektre-labs.
* **License or commercial-rights changes** require the BDFL AND a
  written amendment to `LICENSE-SCSL-1.0.md`. No exceptions.
* **Release tagging** (`v*.*.*` tags on `main`) is the BDFL's sole
  prerogative.

## Conflict resolution

1. Discuss in the PR thread.
2. Escalate to a GitHub Discussion under the `Governance` category.
3. If unresolved, the BDFL decides.

## Good citizenship

* Be specific. A bug report without a minimal reproducer is a
  question, not a bug.
* Be additive. Prefer "add a new kernel" over "refactor v47".
* Respect scope. Kernel-per-feature is not a bug; it is the
  architecture. If you are tempted to merge two kernels, open an
  RFC first.

## Security and disclosure

[SECURITY.md](SECURITY.md) is authoritative. Never open a public
issue for a security bug; email `security@spektre.dev` instead.

---

*Spektre Labs · Creation OS · 2026 · 1 = 1*
