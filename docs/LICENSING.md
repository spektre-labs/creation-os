# Licensing — human-readable explainer

> The **binding text** is `LICENSE-SCSL-1.0.md` (primary) and
> `LICENSE-AGPL-3.0.txt` (fallback under the conditions in
> `LICENSE` §0). This file is **non-binding orientation** for
> humans and AI agents reading the repo.

---

## TL;DR

Creation OS is **source-available**, not "open source" in the
OSI-approved sense, **for the first four years of every Release**.

After four years, every Release **automatically converts** to
**AGPL-3.0-only** (a real OSI-approved open-source license). This
gives a rolling "always eventually free" guarantee.

For those four years, the **Spektre Commercial Source License v1.0
(SCSL-1.0)** governs:

- **Private individuals, academia, non-profits, journalism, and
  reproducibility/security audits use it for free.**
- **For-profit entities with > EUR 1 M revenue, all SaaS / hosted
  services, all government use, all military/intelligence use, and
  all closed-source product integration require a paid Commercial
  License from Spektre Labs Oy.**
- **State actors, sanctioned persons, mass-surveillance operators,
  and aggressors are categorically denied use.**

The combined effect is **stricter than MongoDB SSPL**, **stricter
than Redis RSALv2/SSPLv2**, **stricter than Elastic License v2**,
and **stricter than Business Source License 1.1**, while still
guaranteeing eventual full open-source release after the four-year
Change Date.

---

## Why this license, and not "just AGPL"?

AGPL-3.0 has a well-known weakness: it does **not** prevent a
hyperscaler from offering Creation OS as a hosted service while
keeping the **management plane, scheduler, telemetry, billing, and
hardware-attestation pipeline** all closed. That's exactly the
extraction pattern that defunded MongoDB and Redis as venture-
backed open-core companies and prompted SSPL.

SSPL §13 closes that hole for the **service stack** but stops at
the operating-system boundary. Creation OS targets **silicon-class
deployments** where the line between "kernel" and "infrastructure"
runs through the **hypervisor, the FPGA bitstream, and the
hardware attestation pipeline**. SCSL-1.0 §5 ("Service Source
Disclosure (SSPL §13++)") follows the line all the way down.

ELv2 closes the **anti-circumvention** hole — operators may not
disable license-key checks. SCSL-1.0 §8 generalizes that to the
σ-kernel decision gate, the License-Bound Receipt, and the safety
abstention signals.

PolyForm Strict closes the **commercial-use** hole. SCSL-1.0 §1.8
adopts that posture and adds the EUR 1 M revenue threshold so that
genuinely small for-profits (SaaS-of-one, indie devs going LLC for
tax reasons) are not impeded.

BSL 1.1 closes the **eventual open-source guarantee** — the source
must convert. SCSL-1.0 §12 sets the Change Date to four years.

Hippocratic 3.0 / Anti-996 close the **human-rights** hole. SCSL-1.0
§9 (state-actor / mass-surveillance) and §10 (sanctions / aggression)
make those exclusions concrete and forum-enforceable under Finnish
and EU law.

---

## Decision flowchart

```
                ┌─────────────────────────────────┐
                │  Are you a Sanctioned Person    │
                │  or supporting Aggression?      │
                │  (SCSL §1.15, §1.14)            │
                └──────────┬──────────────────────┘
                           │ YES → DENIED, no license at any price.
                           │
                           ▼
                ┌─────────────────────────────────┐
                │  Are you a State Actor or       │
                │  acting for/under one?          │
                │  (SCSL §1.13, §9.1)             │
                └──────────┬──────────────────────┘
                           │ YES → see §9.3 Sovereign route only;
                           │       §9.1(b) restrictions never waivable.
                           ▼
                ┌─────────────────────────────────┐
                │  Are you a private individual,  │
                │  academic, non-profit, or       │
                │  audit/research user?           │
                │  (SCSL §3.1–§3.6)               │
                └──────────┬──────────────────────┘
                           │ YES → FREE under SCSL-1.0;
                           │       comply with §6 (attribution),
                           │       §7 (trademarks),
                           │       §11 (License-Bound Receipt).
                           ▼
                ┌─────────────────────────────────┐
                │  Is your annual revenue under   │
                │  EUR 1 000 000?                 │
                │  (SCSL §1.8(c))                 │
                └──────────┬──────────────────────┘
                           │ YES → 30-day non-prod evaluation FREE.
                           │       Beyond that → Commercial License.
                           ▼
                ┌─────────────────────────────────┐
                │  Will you offer the software    │
                │  (or its output) as a Service?  │
                │  (SCSL §1.6)                    │
                └──────────┬──────────────────────┘
                           │ YES → either publish the FULL service
                           │       stack source per §5 (within 14 days),
                           │       OR get a Commercial License that
                           │       expressly waives §5.
                           ▼
                ┌─────────────────────────────────┐
                │  All other commercial use       │
                └──────────┬──────────────────────┘
                           ▼
                  Commercial License REQUIRED
                  (COMMERCIAL_LICENSE.md tier matrix).
```

---

## What you CAN do without paying

- Clone the repo, build it on your laptop, hack on it.
- Write a master's thesis or PhD dissertation on it.
- Run a peer-reviewed benchmark and publish the paper.
- Audit the security and write a CVE.
- Run a 30-day evaluation in a non-production sandbox.
- Use any Release that is **older than four years** under AGPL-3.0
  with no further constraint.

## What you CANNOT do without paying

- Embed Creation OS in a closed-source product.
- Offer Creation OS (or its output) as a hosted Service without
  publishing the **complete service-stack source** per SCSL §5.
- Use Creation OS in any internal corporate workflow at a
  for-profit > EUR 1 M revenue beyond the 30-day evaluation.
- Use Creation OS for any **government, intelligence, military, or
  law-enforcement** purpose, except under SCSL §3.6 (publicly
  funded academic research) or under a Sovereign Commercial License
  per §9.3.

## What you cannot do **at any price**

- Use Creation OS for **operational** State-Actor work as defined
  in SCSL §9.1(b) — targeting, kinetic effects, lethal autonomy,
  signals-intelligence collection, mass communications interception,
  biometric mass identification, predictive policing of natural
  persons, or election-influence operations. **The Licensor will
  refuse this license at any price.**
- Use Creation OS to support **Mass Surveillance** (§9.2) as defined
  by EU CFR Articles 7–8 and the CJEU jurisprudence (*Digital
  Rights Ireland*, *Tele2 Sverige*, *Schrems II*).
- Use Creation OS in support of conduct constituting **Aggression,
  war crimes, crimes against humanity, or genocide** (§10).

---

## "Why is this enforceable, in practice?"

Three layers:

### Layer 1 — copyright

SCSL-1.0 is a **copyright** license. Copyright in the kernel,
documentation, and σ-vocabulary belongs to the Licensor. Copyright
licenses bind anyone in the world who copies, distributes, or
prepares derivative works of the Software, **regardless** of whether
they have signed a contract. Conditional copyright grants (the
hallmark of GPL/AGPL/SSPL/SCSL) are routinely upheld by courts
worldwide — see *Jacobsen v. Katzer* (Fed. Cir. 2008), *Versata v.
Ameriprise* (W.D. Tex. 2014), *XimpleWare v. Versata* (N.D. Cal.
2014), and the *Welte/gpl-violations.org* line of German cases.

### Layer 2 — trademark

The names "Spektre", "Spektre Labs", "Creation OS", "σ kernel",
and the σ-mark are trademarks. Use of any Mark in commerce without
authorization is **trademark infringement**, enforceable
independently of any copyright license, in any jurisdiction with a
Madrid-Protocol-aligned trademark regime.

### Layer 3 — License-Bound Receipts (cryptographic evidence)

Per SCSL-1.0 §11, every Receipt emitted by Creation OS carries the
SHA-256 of `LICENSE-SCSL-1.0.md`. This is **cryptographic anchoring
of the license to every emitted artefact** — a property no prior
source-available license has shipped. A published Receipt that
omits or forges the `license_sha256` field is **admissible evidence**
of a §6/§8/§11 breach in any forum, with no further authentication
needed (the hash itself is the authentication).

This makes silent forks, license-removed redistribution, and
"laundered through an LLM" derivation patterns **detectable from
the published artefact alone**.

### Layer 4 — Choice of forum and law

SCSL-1.0 §18 selects **Finnish law and Helsinki courts**, with an
**ICC arbitration option** at the Licensor's election. Both
choices are recognized under EU Regulation 1215/2012 (Brussels I
*bis*) and the 2005 Hague Choice of Court Convention; ICC arbitral
awards are enforceable in 172 states under the 1958 New York
Convention. There is **no forum** for a commercial-use defendant
to escape SCSL-1.0 by venue-shopping.

---

## "What about state actors that just steal the code?"

Three answers, in increasing severity:

1.  **Source-availability does not equal authorization.** That a
    State Actor *can* download the source does not mean it *may*
    use it. SCSL §9 makes the unauthorized-use status explicit.
    Use by a State Actor in violation of §9 is itself a tortious
    act under the UNESCO Recommendation on the Ethics of AI (2021),
    the OECD AI Principles (2019, updated 2024), and arguably the
    ICCPR Article 17.

2.  **The cryptographic evidence layer (§11) makes detection
    possible.** Receipts emitted by Creation OS that surface in
    intelligence-leak archives, FOIA disclosures, or public-policy
    research are self-authenticating evidence of unauthorized use.

3.  **Reputational and procurement consequences.** The Spektre Labs
    canonical license registry publishes the existence of every
    executed Commercial License (not its commercial terms). A State
    Actor that is found using Creation OS without appearing in that
    registry has, by definition, breached the License — and that
    fact can be cited in EU procurement-exclusion proceedings, in
    parliamentary oversight inquiries, and in human-rights
    reporting under ECHR Article 8 and ICCPR Article 17.

In short: SCSL-1.0 cannot **physically prevent** an intelligence
service from copying source code (no license can). It can, and
does, **deny that service legal authority**, **make detection
forensically tractable**, and **create reputational and procurement
exposure** that makes the operational risk of stealing the code
materially higher than the operational risk of paying for a
Sovereign Commercial License.

The latter is exactly what the Licensor offers via §9.3 — to
**democratically governed states bound by EU CFR / ECHR / ICCPR**,
for **named civilian deployments** with a **named human-rights
ombudsperson**, and **publicly registered**. That is the legitimate
path. There is no other.

---

## "What about AI training? Can someone train a model on this and ship it?"

SCSL-1.0 §1.5 **explicitly includes** "fine-tuned, quantized,
ported, transpiled, or otherwise derived (including derivations
through automated agents, large language models, or hardware
synthesis tools)" in its definition of Modified Software. Training
a model on Creation OS source — and shipping the resulting weights
— is a derivation, and it inherits the SCSL-1.0 obligations. If
the model is shipped as a Service, §5 service-source disclosure
applies; if the model is shipped commercially, §4 paid Commercial
License applies; if the model is shipped to circumvent attribution
under §6, that is a §8 breach. There is no "AI training laundering"
exemption.

---

## How does this compare to other source-available licenses?

| License                          | Anti-SaaS extraction        | Source disclosure  | Anti-circumvention | Eventual OSS conversion | Anti-state-actor | Anti-mass-surveillance | Sanctions/aggression | License-bound receipt |
|----------------------------------|-----------------------------|--------------------|--------------------|-------------------------|-------------------|------------------------|----------------------|------------------------|
| AGPL-3.0                         | partial (network copyleft)  | source only        | no                 | already free            | no                | no                     | no                   | no                     |
| MongoDB SSPL v1                  | yes (§13)                   | service stack      | no                 | no                      | no                | no                     | no                   | no                     |
| Redis RSALv2 / SSPLv2            | yes                         | service stack      | no                 | no                      | no                | no                     | no                   | no                     |
| Elastic License v2               | yes (no SaaS resale)        | no                 | yes (key checks)   | no                      | no                | no                     | no                   | no                     |
| Business Source License 1.1      | parameterized               | no                 | no                 | yes (Change Date)       | no                | no                     | no                   | no                     |
| Functional Source License 1.1    | yes                         | no                 | no                 | yes (2-yr DOSP)         | no                | no                     | no                   | no                     |
| PolyForm Strict / Noncommercial  | yes (no commercial use)     | no                 | no                 | no                      | no                | no                     | no                   | no                     |
| Cryptographic Autonomy License   | yes                         | yes (user-data)    | no                 | no                      | no                | no                     | no                   | no                     |
| Hippocratic License 3.0          | partial                     | no                 | no                 | no                      | partial           | partial                | partial              | no                     |
| **Spektre SCSL-1.0**             | **YES (§5, SSPL §13++)**    | **service stack + hypervisor + FPGA + attestation pipeline (§1.7)** | **YES (§8, σ-gate + receipts + abstention)** | **YES (4-yr Change Date → AGPL-3.0)** | **YES (§9)** | **YES (§9.2)** | **YES (§10)** | **YES (§11, SHA-256 of license)** |

This is, to the best of the Licensor's knowledge as of 2026-04-16,
the **broadest source-available license shipped to date**.

---

## Files

- `LICENSE` — top-level dispatcher (read first)
- `LICENSE-SCSL-1.0.md` — binding master license
- `LICENSE-AGPL-3.0.txt` — binding fallback
- `LICENSE.sha256` — pinned hash for License-Bound Receipts
- `NOTICE` — copyright / trademark / patent notice
- `COMMERCIAL_LICENSE.md` — paid-tier matrix
- `CLA.md` — Contributor License Agreement
- `docs/LICENSE_MATRIX.md` — who-may-do-what compatibility matrix
- `tools/license/` — header-application, header-check, hash-pin scripts

---

*Spektre Labs Oy · Creation OS · `docs/LICENSING.md` · 2026-04-16*
