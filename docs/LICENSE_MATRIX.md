# License Compatibility Matrix

> Binding text: `LICENSE-SCSL-1.0.md`. This file is a quick-lookup
> matrix; in case of conflict the SCSL-1.0 governs.

## Who-may-do-what (current Releases, before Change Date)

| Actor                                                              | Personal use | Source modification (private) | Source modification (public, fork) | Internal corporate use | Closed-source product distribution | Hosted Service (SaaS / MaaS / agent-as-a-service) | Government / military / intelligence operational use |
|--------------------------------------------------------------------|:------------:|:-----------------------------:|:----------------------------------:|:----------------------:|:----------------------------------:|:-------------------------------------------------:|:---------------------------------------------------:|
| Private individual (hobby, home, hacker)                           | ✅ free      | ✅ free                       | ✅ free (rename per §7.3 if material change) | n/a               | ❌ requires Commercial             | ❌ requires §5 service-source publish OR Commercial | ❌                                                  |
| Academic researcher (degree-granting institution)                  | ✅ free      | ✅ free                       | ✅ free                            | ✅ free                | ❌                                 | ❌                                                | ⚠ §3.6 only (publicly funded research, no IS/military funding) |
| Independent researcher                                             | ✅ free      | ✅ free                       | ✅ free                            | n/a                    | ❌                                 | ❌                                                | ⚠ §3.6 only                                         |
| Registered non-profit (humanitarian, civic, journalism)            | ✅ free      | ✅ free                       | ✅ free                            | ✅ free                | ⚠ ask first                        | ⚠ §5 publish OR ask                                | ❌                                                  |
| Reproducibility / security auditor                                 | ✅ free      | ✅ free                       | ⚠ disclose responsibly per SECURITY.md | ✅ free          | ❌                                 | ❌                                                | ❌                                                  |
| For-profit ≤ EUR 1 M revenue, ≤ 25 FTE, ≤ 4 yrs old (Startup tier) | n/a          | ✅ 30-day eval                | ⚠ rename per §7.3                  | ⚠ 30-day eval; then **Startup** Commercial | ⚠ **Startup** Commercial      | ⚠ **Startup** Commercial **OR** §5 publish      | ❌                                                  |
| For-profit ≤ EUR 50 M revenue, ≤ 250 FTE (Growth tier)             | n/a          | ⚠ 30-day eval                 | ⚠ rename                           | ⚠ **Growth** Commercial | ⚠ **Growth** Commercial            | ⚠ **Growth** Commercial **OR** §5 publish        | ❌                                                  |
| Larger for-profit (Enterprise tier)                                | n/a          | ⚠ 30-day eval                 | ⚠ rename                           | ⚠ **Enterprise**       | ⚠ **Enterprise**                   | ⚠ **Enterprise** Commercial **OR** §5 publish    | ❌                                                  |
| Cloud hyperscaler                                                  | n/a          | ⚠ 30-day eval                 | ⚠ rename                           | ⚠ Strategic            | ⚠ Strategic                        | ⚠ **Strategic** Commercial **OR** §5 publish-FULL stack including hypervisor, FPGA, attestation pipeline | ❌ |
| Democratic state, civilian deployment, EU CFR / ECHR / ICCPR bound | n/a          | n/a                           | n/a                                | ⚠ **Sovereign** Commercial (§9.3) | n/a                       | ⚠ **Sovereign** Commercial                       | ❌ §9.1(b) NOT waivable under any tier              |
| Intelligence service / military (any state)                        | ❌           | ❌                            | ❌                                 | ❌                     | ❌                                 | ❌                                                | ❌ (refused at any price)                           |
| Sanctioned Person (§1.15)                                          | ❌           | ❌                            | ❌                                 | ❌                     | ❌                                 | ❌                                                | ❌ (categorical denial)                             |
| Aggression actor (§1.14)                                           | ❌           | ❌                            | ❌                                 | ❌                     | ❌                                 | ❌                                                | ❌ (categorical denial)                             |

Legend:
- ✅ permitted under SCSL-1.0 §3 free-of-charge tier
- ⚠ permitted only under specified Commercial License tier or §5 service-source publish
- ❌ prohibited; no commercial license available at any price for the rightmost three rows
- n/a tier irrelevant to that actor

## After Change Date (4 years from Release publication)

That specific Release is automatically AGPL-3.0-only. The following
SCSL-1.0 sections **survive** the Change Date:

- §6 Attribution and SPDX headers
- §7 Trademarks
- §10 Sanctions and Aggression (as standing notice; enforced under
  applicable export-control / sanctions law)

All other restrictions (§4 paid Commercial License, §5 Service
Source disclosure, §8.1(c)–(e) σ-gate / receipt / attestation
anti-circumvention, §9 State-Actor / Mass-Surveillance, §11
License-Bound Receipt) **fall away for that converted Release**
(but remain in force for any later Release that has not yet
reached its own Change Date).

## Combining with third-party code

| Third-party license             | Compatibility with SCSL-1.0 + AGPL-3.0 dual                              |
|---------------------------------|---------------------------------------------------------------------------|
| MIT, BSD-2/3-Clause, ISC, Apache-2.0 | ✅ Compatible; carry the upstream notice; SCSL still binds the combined work |
| LGPL-2.1+, LGPL-3.0+            | ✅ Compatible if linked dynamically; carry upstream notice                |
| MPL-2.0                         | ✅ Compatible (file-level copyleft); carry upstream notice                |
| GPL-2.0-only                    | ⚠ NOT compatible with AGPL-3.0; do not import GPL-2.0-only code          |
| GPL-2.0-or-later, GPL-3.0       | ✅ Compatible (use AGPL-3.0 path); carry upstream notice                  |
| AGPL-3.0                        | ✅ Compatible (this is the fallback); carry upstream notice               |
| MongoDB SSPL v1                 | ⚠ Compatible only if combined work also satisfies SCSL §5 service-stack disclosure |
| BUSL 1.1, FSL 1.1, ELv2, PolyForm Strict | ⚠ Mixing source-available licenses requires legal review; ask first |
| Public domain (CC0, Unlicense)  | ✅ Compatible                                                              |

When in doubt, **ask before importing**:
`spektrelabs@proton.me`.

## Spektre Labs Marks at a glance

The Marks (§7) are **never** licensed by SCSL or AGPL. To use
"Spektre", "Spektre Labs", "Creation OS", "σ kernel", or
"Lauri Elias Rainio" in commerce — including in product names,
domain names, repository names, app-store names, package names, or
trademarks — You need a **separate written trademark license**.

For factual attribution (e.g., "this product is built on Creation
OS by Spektre Labs Oy"), no separate trademark license is required.

---

*Spektre Labs Oy · Creation OS · `docs/LICENSE_MATRIX.md` · 2026-04-16*
