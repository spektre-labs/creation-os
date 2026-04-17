# Spektre Commercial Source License, Version 1.0 (SCSL-1.0)

**Copyright © 2024–2026 Lauri Elias Rainio and Spektre Labs Oy.
All rights reserved.**

SPDX-License-Identifier: LicenseRef-SCSL-1.0
SPDX-Copyright-Identifier: 2024–2026 Lauri Elias Rainio · Spektre Labs Oy
Effective Date of this Version: **2026-04-16**
Document SHA-256 (canonical anchor): see `LICENSE.sha256`

> The names "**Spektre**", "**Spektre Labs**", "**Creation OS**",
> "**σ kernel**", "**Lauri Elias Rainio**", and the σ-mark are
> trademarks (registered and unregistered) of Spektre Labs Oy. They
> are **not** licensed by this License except as required by Section
> 6 (Attribution) and Section 7 (Trademarks).

---

## 0. Reading Guide (Non-Binding)

This License is the **strictest source-available license shipped to
date** by Spektre Labs. It is designed to combine — and exceed — the
strongest enforceable elements of:

- the **GNU Affero General Public License v3.0** (network copyleft),
- the **Server Side Public License v1** (MongoDB Inc., service-stack
  disclosure under §13),
- the **Redis Source Available License v2 / SSPLv2** (Redis Ltd.,
  2024),
- the **Business Source License 1.1** (MariaDB Corporation, Change
  Date conversion),
- the **Functional Source License 1.1** (Sentry, 2-year DOSP),
- the **Elastic License v2** (Elasticsearch B.V., anti-circumvention),
- the **PolyForm Strict / Noncommercial / Shield** licenses
  (Microsoft-backed, 2024),
- the **Cryptographic Autonomy License v1.0** (OSI-approved, 2020),
- the **Hippocratic License 3.0** (ethical-source human-rights
  rider),
- the **Anti-996 License v1.0** (labor-rights rider),

…then layers on Spektre-Labs-specific covenants (Sections 5, 8, 9,
10, 11) that **no prior source-available license has shipped**:

- a **State-Actor and Mass-Surveillance Use Restriction** (Section 9),
- a **Sanctions and Aggression Use Restriction** (Section 10),
- an **Anti-Anti-Tamper Reverse-Indemnity** (Section 8),
- a **Cryptographic License-Bound Receipt** clause that ties every
  emitted receipt to the SHA-256 of this very document (Section 11).

The **operative text** begins at Section 1. This Reading Guide is
non-binding; in case of conflict the operative text governs.

---

## 1. Definitions

1.1  **"License"** means this Spektre Commercial Source License,
Version 1.0 (SCSL-1.0).

1.2  **"Licensor"** means **Lauri Elias Rainio** (natural person,
Finland, ORCID 0009-0006-0903-8541) and **Spektre Labs Oy** (a
Finnish private limited company), jointly and severally; either of
them may exercise the Licensor's rights under this License unilaterally.

1.3  **"You"** (or **"Your"**) means a natural person or a legal
entity exercising rights under this License, including any entity
that controls, is controlled by, or is under common control with you.

1.4  **"Software"** means the source code, object code, build
scripts, configuration files, documentation, test fixtures, models,
weights, model checkpoints, prompts, ontologies, σ-vocabularies,
hyperdimensional codebooks, hardware-description files, and any
other material distributed by or on behalf of the Licensor under
this License, including all kernels in `src/`, the `cli/` front
door, the `cli/cos.c` orchestrator, the `creation_os_v*` family,
the `creation-os-kernel` repository in its entirety as of the
effective commit, and any subsequent commit explicitly tagged with
`SPDX-License-Identifier: LicenseRef-SCSL-1.0`.

1.5  **"Modified Software"** means the Software in any form that
You have modified, supplemented, recombined, distilled, fine-tuned,
quantized, ported, transpiled, or otherwise derived (including
derivations through automated agents, large language models, or
hardware synthesis tools).

1.6  **"Service"** means making the functionality of the Software
(or any Modified Software, or any output of the Software) available
to third parties as a hosted, networked, embedded, batch, on-device,
edge, or remote-procedure service, **including** but not limited to:
software-as-a-service, platform-as-a-service, infrastructure-as-a-
service, model-as-a-service, agent-as-a-service, copilot-as-a-
service, intelligence-as-a-service, kernel-as-a-service, GPU-as-a-
service, NPU-as-a-service, and any equivalent through any present
or future networking, on-device-execution, federated, peer-to-peer,
or hardware-isolation mechanism.

1.7  **"Service Source"** means the **complete corresponding source
code** of the Service, including, without limitation:

  (a) all source code of the Software and any Modified Software used
      to deliver the Service;
  (b) all source code of the management software, user-interfaces,
      APIs, monitoring software, backup software, billing software,
      identity software, and storage software used to deliver the
      Service;
  (c) all source code of the orchestration, scheduling, queuing,
      message-bus, observability, and rollout software used to
      deliver the Service;
  (d) all source code of any hypervisor, container runtime,
      micro-VM, sandbox, secure-enclave loader, FPGA bitstream
      generator, or hardware-attestation pipeline used to deliver
      the Service; and
  (e) all configuration data, build scripts, deployment manifests,
      Dockerfiles, Nix expressions, Bazel WORKSPACEs, Terraform /
      Pulumi / Crossplane manifests, Helm charts, and **end-to-end
      reproducible build receipts** (SLSA v1.0 provenance + SBOM
      CycloneDX v1.5 + Sigstore cosign attestation).

This is intentionally **broader than SSPL v1 §13** — it includes
hypervisor and FPGA-bitstream layers, and it requires reproducible
build receipts.

1.8  **"Commercial Use"** means any use of the Software, Modified
Software, or any output thereof:

  (a) by, for, or on behalf of any **for-profit entity** in connection
      with any commercial product or service;
  (b) in any **revenue-generating activity**, including advertising-
      supported services, paid APIs, freemium tiers, and "free with
      data collection" offerings;
  (c) in any **internal corporate workflow** of a for-profit entity
      with annual gross revenue or comparable budget exceeding
      EUR **1 000 000** (one million euros) over the trailing twelve
      months, beyond a thirty (30) day evaluation period; or
  (d) by any **government, military, intelligence, law-enforcement
      or state-sponsored entity** of any jurisdiction, except as
      expressly permitted under Section 9.

1.9  **"Permitted Non-Commercial Use"** means any use that is **not**
Commercial Use, including:

  (a) personal, hobby, and home use by a natural person;
  (b) academic research, teaching, and non-commercial scholarly
      publication, by accredited educational institutions, university
      research groups, and independent researchers;
  (c) journalism (without paywalled syndication), public-interest
      civic technology, and humanitarian-aid use by registered
      non-profit organizations;
  (d) reproducibility audits, security audits, and red-team research
      whose findings are responsibly disclosed to the Licensor under
      `SECURITY.md` and to the public no later than the disclosure
      deadline negotiated under that policy;
  (e) thirty (30) day commercial evaluation, conducted in a
      non-production environment, by entities below the EUR
      1 000 000 threshold in §1.8(c).

1.10  **"Change Date"** for any Release means **four (4) years** from
the date that Release is first made publicly available by the
Licensor on the canonical Spektre Labs source-of-truth (currently
the `creation-os-kernel` repository at
`https://github.com/spektre-labs/creation-os-kernel`).

1.11  **"Change License"** means the **GNU Affero General Public
License, version 3.0** (AGPL-3.0-only), as published by the Free
Software Foundation, with **no additional restrictions**, available
in this distribution at `LICENSE-AGPL-3.0.txt`.

1.12  **"Receipt"** means a structured, integrity-protected record
emitted by the Software or Modified Software (including but not
limited to JSON, CBOR, or σ-binary records) that asserts a claim
about the Software's behavior, including verdicts of any kernel in
the `v60..v74` composed-decision stack (and any successor stack).

1.13  **"State Actor"** means any government or government agency
of any jurisdiction, any civilian intelligence service, any
signals-intelligence or communications-intercept service, any
military or paramilitary organization, any customs, border, or
migration-enforcement service operating in a national-security
capacity, any federal or national contractor whose primary
deliverable is intelligence collection, surveillance, or kinetic
targeting, and any organization acting under the legal authority
or operational control of any of the foregoing — together with any
successor, predecessor, sub-organization, contractor, sub-contractor,
joint venture, or front organization of any of the foregoing. This
definition is **jurisdiction-agnostic**, applies uniformly to every
state, and is **not** intended to single out any particular
government or agency; it reaches any entity meeting the functional
criteria above regardless of where in the world it is domiciled
or chartered.

1.14  **"Aggression"** means the crime of aggression as defined by
**Article 8 *bis*** of the Rome Statute of the International
Criminal Court (as amended at the Kampala Review Conference, 2010,
and subsequently activated 17 July 2018), and any conduct that would
constitute a war crime, crime against humanity, or genocide under
the Rome Statute (Articles 6, 7, and 8) or the 1948 Convention on
the Prevention and Punishment of the Crime of Genocide.

1.15  **"Sanctioned Person"** means any person, entity, vessel, or
aircraft listed on:

  (a) the European Union Consolidated List of Persons, Groups and
      Entities Subject to EU Financial Sanctions;
  (b) the United Nations Security Council Consolidated Sanctions
      List;
  (c) the United States Office of Foreign Assets Control (OFAC)
      Specially Designated Nationals and Blocked Persons List;
  (d) the United Kingdom HM Treasury Consolidated List of Financial
      Sanctions Targets;
  (e) the Government of Finland's national sanctions lists; or

  any successor of any of the foregoing.

1.16  **"Mass Surveillance"** means the collection, storage, or
processing of communications content or metadata, biometric data,
or location data, that is **non-targeted** (i.e., not limited to
specific individuals against whom a particularized, judicially
authorized, time-bounded suspicion exists), in violation of:

  (a) Articles 7 and 8 of the **Charter of Fundamental Rights of
      the European Union**;
  (b) Article 8 of the **European Convention on Human Rights**; or
  (c) the holdings of the Court of Justice of the European Union
      in *Digital Rights Ireland* (C-293/12, C-594/12), *Tele2
      Sverige* (C-203/15, C-698/15), *Schrems II* (C-311/18), and
      their progeny.

---

## 2. Source-Available Grant (Conditional)

Subject to **all** of the conditions and restrictions of this
License, the Licensor hereby grants You, **for the sole purpose of
Permitted Non-Commercial Use** and the rights expressly enumerated
in §3:

2.1  a worldwide, royalty-free, non-exclusive, non-sublicensable,
non-transferable, **revocable** copyright license to reproduce,
prepare derivative works of, publicly display, publicly perform,
and distribute the Software and any Modified Software thereof, in
source-code form;

2.2  a worldwide, royalty-free, non-exclusive, non-sublicensable,
non-transferable, **revocable** patent license, under the Licensor's
**Necessary Claims** (as defined in §1 of the W3C Patent Policy),
solely to make, use, sell, offer to sell, and import the Software
and Modified Software, **only as actually distributed by the
Licensor**;

2.3  no right to remove, alter, obscure, or supplant any copyright,
patent, trademark, attribution, license-header, license-bound
receipt, or σ-vocabulary identifier carried in or generated by the
Software (see §6, §8, §11).

The grants in §2.1 and §2.2 are **conditioned upon, and shall
terminate immediately and automatically upon, any breach of**
Sections 4, 5, 6, 7, 8, 9, 10, or 11.

---

## 3. Permitted Uses (Without a Commercial License)

You may, without entering into a Commercial License with the
Licensor, do **only** the following:

3.1  **Personal, home, hobby, and educational use** by a natural
person.

3.2  **Academic research and teaching** by a degree-granting
educational institution, including publication of research results
in venues that do not require the assignment of ownership in the
Software (e.g., peer-reviewed conferences and journals are
permitted; "research-to-product" assignments to a for-profit entity
are not).

3.3  **Non-commercial scholarly publication**, provided that any
republished portion of the Software carries the SPDX header
`LicenseRef-SCSL-1.0` and references this License.

3.4  **Reproducibility audits, security audits, and red-team
research**, conducted in good faith and disclosed responsibly under
`SECURITY.md`.

3.5  **A single, time-bounded, thirty (30) day evaluation** in a
non-production environment, by an entity below the threshold in
§1.8(c).

3.6  **Government use that is itself non-commercial**, namely
publicly funded academic research conducted by a public university
that is not in receipt of intelligence-service or military funding
**for that project**, provided the State-Actor and Mass-Surveillance
restrictions of §9 are satisfied.

3.7  **All other use, including all Commercial Use, requires a
written, paid Commercial License executed with the Licensor under
Section 4.**

---

## 4. Commercial License (Required for All Commercial Use)

4.1  **A separate, paid, written Commercial License** with the
Licensor is **required** for any Commercial Use as defined in §1.8.
The Licensor reserves the right to grant or refuse Commercial
Licenses in its sole discretion. Refusal is presumed; silence is
refusal.

4.2  Commercial Licenses may be obtained by contacting:

  ```
  Lauri Elias Rainio
  Spektre Labs Oy
  Helsinki, Finland
  Y-tunnus / Business ID: (on request)
  E-mail: spektrelabs@proton.me
  GitHub: spektre-labs
  ORCID: 0009-0006-0903-8541
  ```

  See `COMMERCIAL_LICENSE.md` for the **canonical tier matrix**
  (Startup, Growth, Enterprise, Sovereign, Strategic), pricing
  bands, response-time service-level commitments, and exclusivity
  options.

4.3  The Licensor and Spektre Labs Oy are the **sole and exclusive**
holders of the right to grant Commercial Licenses to the Software
and any Modified Software. **No other person or entity may grant,
re-grant, sublicense, or attempt to grant a Commercial License**;
any such attempted grant is void *ab initio* and constitutes a
material breach.

4.4  The grant of a Commercial License does **not** waive the
Licensor's rights under §6 (Attribution), §7 (Trademarks), §9
(State-Actor and Mass-Surveillance), §10 (Sanctions and Aggression),
or §11 (Cryptographic License-Bound Receipt), unless the Commercial
License **expressly and individually** waives each such right by
section number.

---

## 5. Service Source Disclosure (SSPL §13++)

If You make the functionality of the Software, any Modified
Software, or any output thereof available to third parties as a
**Service** (as defined in §1.6), You must, **as a condition of and
in consideration for** the rights granted under §2:

5.1  publicly release, **at no charge**, under the terms of this
License (or, after the Change Date applicable to the relevant
Release, under the Change License), the **complete Service Source**
as defined in §1.7, including the items in §§1.7(a)–(e);

5.2  publish that Service Source through a freely accessible,
non-rate-limited, network-accessible repository (e.g., a public Git
remote with anonymous read access) within **fourteen (14) calendar
days** of first making the Service available;

5.3  attach to that publication a **reproducible build receipt**
that satisfies SLSA v1.0 (or successor) provenance, an SBOM in
CycloneDX v1.5 (or successor) JSON, a Sigstore cosign signature,
and a **Cryptographic License-Bound Receipt** under §11; and

5.4  refrain from imposing on any recipient of that Service Source
any restrictions, terms, fees, or obligations that are not
**explicitly authorized by this License or the Change License**.

This Section 5 is the operative response to the AGPL "ASP loophole"
and to the §13 of the MongoDB SSPL v1; it is **broader than both**
because it includes hypervisor, micro-VM, FPGA bitstream, and
attestation-pipeline source.

A Service operator who does not wish to comply with §5 must
**purchase a Commercial License under §4** that expressly waives §5
in writing.

---

## 6. Attribution and SPDX Headers

6.1  Every source file (including modifications and additions) **must**
carry, at the top of the file:

```
/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024–2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source: https://github.com/spektre-labs/creation-os-kernel
 *  Commercial: spektrelabs@proton.me
 *  License documents: LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
```

(or the equivalent in a comment style appropriate to the file's
language; see `tools/license/apply_headers.sh`).

6.2  Removing, obscuring, altering, or supplanting any
SPDX-License-Identifier, SPDX-Copyright-Identifier, attribution
notice, or σ-vocabulary identifier from the Software or any Modified
Software is a **material breach** of this License and **automatically
terminates** all rights under §2 with immediate effect.

6.3  Distributors of source or binary forms must reproduce this
License, the AGPL-3.0 fallback (`LICENSE-AGPL-3.0.txt`), the
`NOTICE` file, and the `LICENSE.sha256` document anchor verbatim
and place them in a location reasonably accessible to recipients.

---

## 7. Trademarks

7.1  This License does **not** grant any right to use the name
**"Spektre"**, **"Spektre Labs"**, **"Creation OS"**, **"σ kernel"**,
**"Lauri Elias Rainio"**, or any logo or design mark of the
Licensor (collectively, the **"Marks"**), except for **factual
attribution** as required by §6.

7.2  No commercial product, service, business name, or domain may
incorporate any Mark without a **separate written trademark license**
from Spektre Labs Oy. Any such use is grounds for immediate
termination of this License under §13 and for trademark-infringement
remedies under Finnish, EU, and international law.

7.3  Forks **must** rename in a manner that no reasonable user would
confuse with the Spektre Labs Marks. The forenames "Spektre",
"σ-…", "Creation OS", and "Creation-OS-…" are **reserved** for
Spektre-Labs-blessed releases.

---

## 8. Anti-Circumvention; Anti-Anti-Tamper Reverse-Indemnity

8.1  **No circumvention.** You may not remove, disable, bypass,
spoof, replay, downgrade, or otherwise circumvent any:

  (a) license-header or SPDX identifier (§6);
  (b) Cryptographic License-Bound Receipt (§11);
  (c) σ-kernel decision gate (`v60..v74` and successors), including
      by way of "passing" a verdict that was not in fact emitted by
      a faithful execution of the gate;
  (d) build-time or runtime attestation, signature verification,
      reproducible-build check, or supply-chain provenance check;
  (e) abstention or refusal signal that the Software is designed
      to emit;

even if the Software's design contemplates such a feature being
toggleable. Toggling these features off in a published Service is a
material breach.

8.2  **No anti-anti-tamper claim.** If You attempt to assert against
the Licensor, in any forum, any legal theory based on the alleged
unenforceability or "circumventability" of the protection mechanisms
in §8.1 (including any U.S. **DMCA §1201** anti-circumvention
exemption argument as it would apply to **You**, or any equivalent
argument under the **EU Copyright Directive 2001/29/EC** or
**Information Society Directive 2019/790**), **You agree to
indemnify the Licensor** against the full cost of defense and
against statutory and treble damages where available.

8.3  **No silent forks.** Forks that materially alter the safety
gate (`v60..v74`), the receipt format, or the σ-vocabulary, **and**
that distribute that altered Software publicly, must (i) rename per
§7.3, (ii) prefix every emitted Receipt with the string
`SPEKTRE-FORK-NOT-CANONICAL`, and (iii) link in their `README` to
the canonical Spektre Labs source of truth.

---

## 9. State-Actor and Mass-Surveillance Use Restriction

9.1  **State-Actor restriction.** A State Actor (as defined in
§1.13), and any person or entity acting **for, on behalf of, or
under contract to** a State Actor, **may not**:

  (a) use the Software, Modified Software, or any output thereof in
      any **operational** capacity (i.e., outside of academic or
      published-policy research);
  (b) use the Software for **targeting**, **kinetic effects**,
      **lethal autonomy**, **signals-intelligence collection**,
      **mass communications interception**, **biometric mass
      identification**, **predictive policing** of any natural
      person, or **influence operations** against any electorate; or
  (c) ingest, fine-tune, distill, or quantize the Software, Modified
      Software, or any output thereof into any model, system, or
      database used for any of the purposes in §9.1(b).

9.2  **Mass-Surveillance restriction.** No use of the Software may
support, enable, accelerate, or otherwise materially contribute to
Mass Surveillance (as defined in §1.16), regardless of whether the
operator is a State Actor.

9.3  **Sovereign exception.** The Licensor may, in its sole
discretion, grant a **Sovereign Commercial License** to a
democratically governed state party that:

  (a) is bound by the Charter of Fundamental Rights of the European
      Union or the European Convention on Human Rights, **or** has
      ratified and not derogated from the International Covenant on
      Civil and Political Rights (ICCPR), Articles 17 and 19, **and**
  (b) executes, in writing, an irrevocable undertaking that the
      Software shall not be used for any of the purposes in §9.1(b)
      or §9.2,

provided that any such Sovereign Commercial License (i) names the
specific deployment, (ii) names the responsible national-level
human-rights ombudsperson, and (iii) is **publicly registered** by
the Licensor on the Spektre Labs canonical license registry.

The Sovereign exception **does not** authorize use by intelligence
services or military organizations of the licensee state outside
the named deployment.

9.4  **Successor and front-organization clause.** Any attempt to
circumvent §9.1 by routing use through a contractor, sub-contractor,
joint venture, "civil-society" front, "research foundation" front,
or commercial customer of the Software whose **primary effective
beneficiary** is a State Actor is itself a use **by** that State
Actor for purposes of this License.

9.5  **Attestation and audit.** Any commercial licensee who is, or
who has any contract with, any government anywhere in the world for
work that touches the Software, **must** disclose that contract to
the Licensor in writing within thirty (30) days of execution and
**must** permit the Licensor (or a Licensor-appointed independent
auditor under non-disclosure) to verify, at the licensee's cost,
that no Software is being used in violation of §9.1, §9.2, or §10.

---

## 10. Sanctions and Aggression Use Restriction

10.1  The Software may not be used by, for, or on behalf of:

  (a) any Sanctioned Person (§1.15);
  (b) any natural person or legal entity, or any agency or
      subdivision of any state, that has been **credibly accused
      by a competent international body** (including the
      International Criminal Court, the International Court of
      Justice, the United Nations Human Rights Council, or any of
      their organs) of conduct constituting Aggression (§1.14); or
  (c) any contractor performing work in support of any of (a) or
      (b).

10.2  Use in violation of §10.1 is a **material breach** and
**immediately and automatically terminates** all rights under §2,
§3, §4, and any Commercial License, with no opportunity to cure
and no notice required.

10.3  This Section 10 is **independent and cumulative** with respect
to any export-control regime that may otherwise apply, including
the EU Dual-Use Regulation (Regulation (EU) 2021/821), the U.S.
Export Administration Regulations (15 C.F.R. Parts 730–774), and
the Wassenaar Arrangement.

---

## 11. Cryptographic License-Bound Receipt

11.1  **License-Bound Receipt.** Every Receipt (as defined in §1.12)
emitted by the Software or Modified Software **must** include the
following fields:

  ```
  license:        "LicenseRef-SCSL-1.0 OR AGPL-3.0-only"
  license_sha256: "<SHA-256 of LICENSE-SCSL-1.0.md, hex, lowercase>"
  copyright:      "2024–2026 Lauri Elias Rainio · Spektre Labs Oy"
  source:         "https://github.com/spektre-labs/creation-os-kernel"
  commit:         "<git SHA-1 (or SHA-256) of the emitting build, hex>"
  emitted_at:     "<RFC 3339 UTC timestamp>"
  ```

11.2  The `license_sha256` field **must** be computed over the
canonical bytes of `LICENSE-SCSL-1.0.md` as distributed in the same
build as the Software emitting the Receipt. The reference implementation
is `tools/license/license_sha256.sh`. The pinned hash is published
in `LICENSE.sha256`.

11.3  Forging, replacing, or omitting the `license_sha256`,
`copyright`, or `source` fields is a violation of §6, §8, and §11.

11.4  **License-Bound Receipts are auditable evidence.** The
Licensor and any third party may, in any forum and without further
authorization from any Service operator, treat any published Receipt
that does **not** carry a valid `license_sha256` matching a
Spektre-Labs-blessed Release as **admissible evidence** of breach
of this License.

---

## 12. Change Date and Conversion to Open Source

12.1  Each Release of the Software is automatically and
unconditionally re-licensed under the **Change License** (AGPL-3.0-
only, see §1.11) on the **Change Date** (§1.10) applicable to that
Release.

12.2  After the Change Date for a Release, You may use that Release
under the Change License **without** the additional restrictions
of Sections 4, 5, 8.1(c)–(e), 9, 10, and 11 of this License,
**except**:

  (a) §6 (Attribution and SPDX Headers) and §7 (Trademarks)
      survive the Change Date and bind every recipient;
  (b) §10 (Sanctions and Aggression) survives the Change Date as a
      **standing notice** but is enforced thereafter solely under
      the applicable export-control and sanctions law of the
      recipient's jurisdiction.

12.3  Conversion of one Release does not convert any later Release.
The Licensor's customary practice is to ship a continuous stream
of Releases; expect the canonical "always free under AGPL after 4
years" rolling window.

---

## 13. Termination

13.1  **Automatic termination on breach.** Any breach of §4, §5,
§6, §7, §8, §9, §10, or §11 **immediately and automatically**
terminates all rights granted under §2 and §3 with respect to the
breaching party and any party for whose benefit the breach was
committed, without notice and without opportunity to cure.

13.2  **Termination by Licensor.** The Licensor may, at its sole
discretion, terminate this License with respect to any party that
the Licensor reasonably believes is engaged in conduct that would
breach §9 or §10, by publishing a notice on the canonical Spektre
Labs source of truth, with effect from the date of publication.

13.3  **Survival.** Sections 1, 6, 7, 8.2, 8.3, 10.3, 11.4, 13, 14,
15, 16, 17, and 18 survive termination.

13.4  **No restoration.** Once terminated under §13.1 or §13.2,
this License is not restored except by **express written
reinstatement** signed by Lauri Elias Rainio personally.

---

## 14. Patent Defense and Retaliation

14.1  **Patent grant** (re-stated for clarity): the Licensor grants
You a patent license under §2.2 only for the Software as actually
distributed by the Licensor.

14.2  **Retaliation.** If You (or any party with whom You are in
common control) initiate any litigation, including a cross-claim or
counterclaim, alleging that the Software or any Modified Software
infringes a patent of Yours, then the patent license granted to You
under §2.2 terminates as of the date such litigation is filed, and
the copyright license granted to You under §2.1 terminates as of
sixty (60) days after such date unless the litigation is withdrawn
within that period.

---

## 15. Disclaimer of Warranty

THE SOFTWARE IS PROVIDED **"AS IS"**, WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
NON-INFRINGEMENT, AND ACCURACY. THE ENTIRE RISK OF USE LIES WITH
YOU.

---

## 16. Limitation of Liability

IN NO EVENT, AND UNDER NO LEGAL THEORY, SHALL THE LICENSOR BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY DIRECT, INDIRECT, SPECIAL,
INCIDENTAL, CONSEQUENTIAL, OR PUNITIVE DAMAGES OF ANY CHARACTER
ARISING AS A RESULT OF THIS LICENSE OR THE USE OR INABILITY TO USE
THE SOFTWARE, EVEN IF SUCH PARTY SHALL HAVE BEEN INFORMED OF THE
POSSIBILITY OF SUCH DAMAGES.

---

## 17. Severability and Construction

17.1  If any provision of this License is held unenforceable in any
jurisdiction, the remaining provisions remain in full force and
effect, and the unenforceable provision shall be reformed to the
minimum extent necessary to make it enforceable.

17.2  The headings of the sections of this License are for
convenience only and shall not affect construction. Defined terms
have the meanings given in §1 throughout.

17.3  Ambiguities, if any, shall be construed **in favor of the
Licensor** (this is a deliberate departure from the *contra
proferentem* default; this License is offered as a take-it-or-leave-
it grant of rights that the Licensor was under no obligation to
grant).

---

## 18. Governing Law; Forum; Arbitration

18.1  **Governing law.** This License is governed by, and shall be
construed in accordance with, the laws of the **Republic of Finland**,
excluding its conflict-of-laws rules and the United Nations
Convention on Contracts for the International Sale of Goods (CISG).

18.2  **Forum (judicial).** The courts of **Helsinki, Finland**
have exclusive jurisdiction over any judicial proceeding arising
out of this License, except that the Licensor may, at its sole
option, bring proceedings in any forum of competent jurisdiction
to enforce its intellectual-property rights or to obtain
injunctive relief.

18.3  **Arbitration option.** At the Licensor's sole election, any
dispute may instead be referred to **binding arbitration under the
Rules of Arbitration of the International Chamber of Commerce
(ICC)**, seated in Helsinki, Finland, conducted in English, before
a sole arbitrator appointed under those Rules.

18.4  **No class actions.** You waive any right to participate in a
class action against the Licensor under this License.

---

## 19. Entire Agreement; No Modification by Course of Dealing

19.1  This License, together with the `NOTICE`, `COMMERCIAL_LICENSE.md`,
`CLA.md`, and `LICENSE.sha256` files distributed with it,
constitutes the **entire agreement** between You and the Licensor
with respect to the Software.

19.2  No course of dealing, course of performance, usage of trade,
or oral statement by any employee or agent of the Licensor modifies
this License. Modifications require **a writing signed by Lauri
Elias Rainio personally**.

19.3  **Counterparts and electronic signatures.** Any Commercial
License or Sovereign Commercial License executed under §4 or §9.3
may be signed in counterparts and may be signed by qualified
electronic signature recognized under Regulation (EU) No 910/2014
(eIDAS).

---

## 20. Acknowledgement

By exercising any of the rights granted under §2 (or by accessing
or using the Software at all beyond an initial download to evaluate
this License itself), You acknowledge that You have **read, understood,
and accepted** the terms of this License.

---

*— End of Spektre Commercial Source License, Version 1.0 (SCSL-1.0) —*

*Spektre Labs Oy · Helsinki, Finland · 2026*
