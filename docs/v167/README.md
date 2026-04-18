# v167 — σ-Governance-API: organization-level control plane

## Problem

Single user = single machine.  An organization that wants to
*run* Creation OS needs more: domain-scoped policies, a ledger
of which node is running which adapter with which policy
version, an audit trail that survives GDPR / SOC2 / HIPAA
review, and role-based access so an auditor can *see* the
decisions without *making* them.

## σ-innovation

- **Domain policies.**  Each policy carries `(domain, τ,
  abstain_message, require_sandbox)`.  `abstain_message` can
  be `null` (creative domain: free exploration) or a mandated
  phrase (medical: *"Consult a professional."*).
  `require_sandbox` forces every emit through a sandbox
  (v164 plugin sandbox).

- **Fleet ledger.**  A node stamps its `policy_version` at
  every push.  A **down** node is refused the stamp — so an
  unhealthy node cannot claim compliance with the latest
  policy.  That invariant is a compliance-grade property.

- **Append-only audit log.**  Ring buffer of the last 64
  decisions, each tagged with `(actor, role, node_id,
  domain, prompt, verdict, sigma_product, note)`.  Every
  `denied` verdict has a clear `note` explaining *which*
  capability was missing.

- **Four RBAC roles.**  `admin` (all), `user` (chat only),
  `auditor` (read-only audit), `developer` (chat + plugin
  install + kernel enable).  The `auditor` is *deliberately*
  denied chat so the compliance seat cannot become the
  prompting seat.

## Merge-gate

`benchmarks/v167/check_v167_governance_policy_apply.sh`
asserts:

1. `--self-test` is 0
2. policies contain medical / creative / code / legal with
   correct τ and `require_sandbox`
3. `creative.abstain_message` is JSON `null`
4. fleet has 4 nodes, stamped `policy_version`
5. a `down` node lags the active version after a push
6. audit log contains at least one of each verdict
7. auditor invocations are `denied`
8. adapter rollback applies everywhere
9. byte-identical state JSON + audit NDJSON across runs

## v167.0 vs v167.1

| Aspect | v167.0 (this release) | v167.1 (planned) |
|---|---|---|
| Policy distribution | In-process state transitions | HTTP policy server + WebSocket push (v166) |
| Auth | Role enum | TLS client certs, SSO |
| Audit export | NDJSON to stdout | GDPR / SOC2 / HIPAA profiles |
| Fleet discovery | Baked 4 nodes | v150 swarm autodiscovery |

## Honest claims

- **Lab demo (C)**: deterministic policy-store + fleet-stamping
  + audit-appender + RBAC-enforcer.
- **Not yet**: real HTTP server, real TLS, real SSO, real
  compliance-certified exports.
- v167.0 *freezes the data shapes and the enforcement invariants*;
  v167.1 *distributes them over the wire*.
