# v257 — σ-Locale (`docs/v257/`)

Localisation and cultural awareness.  Creation OS runs
worldwide.  v257 types the locale surface: language,
culture, legal compliance, and time formatting — each
with a σ.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Languages (exactly 10, canonical order)

| locale | timezone               | date_format  | currency | σ_language |
|--------|------------------------|--------------|----------|-----------:|
| `en`   | `America/Los_Angeles`  | `MM/DD/YYYY` | `USD`    | 0.08       |
| `fi`   | `Europe/Helsinki`      | `DD.MM.YYYY` | `EUR`    | 0.11       |
| `zh`   | `Asia/Shanghai`        | `YYYY-MM-DD` | `CNY`    | 0.16       |
| `ja`   | `Asia/Tokyo`           | `YYYY/MM/DD` | `JPY`    | 0.14       |
| `de`   | `Europe/Berlin`        | `DD.MM.YYYY` | `EUR`    | 0.09       |
| `fr`   | `Europe/Paris`         | `DD/MM/YYYY` | `EUR`    | 0.10       |
| `es`   | `Europe/Madrid`        | `DD/MM/YYYY` | `EUR`    | 0.09       |
| `ar`   | `Asia/Riyadh`          | `DD/MM/YYYY` | `SAR`    | 0.21       |
| `hi`   | `Asia/Kolkata`         | `DD/MM/YYYY` | `INR`    | 0.19       |
| `pt`   | `America/Sao_Paulo`    | `DD/MM/YYYY` | `BRL`    | 0.12       |

### Cultural style (v202 culture, exactly 3)

| style          | example locale |
|----------------|----------------|
| `direct`       | `en`           |
| `indirect`     | `ja`           |
| `high_context` | `ar`           |

Every `example_locale` must be one of the 10 declared
locales; drift is a gate failure.

### Legal compliance (exactly 3 regimes, v199 law)

| regime             | jurisdiction | compliant | last_reviewed |
|--------------------|--------------|:---------:|--------------:|
| `EU_AI_ACT`        | `EU`         | true      | 2026-04-01    |
| `GDPR`             | `EU`         | true      | 2026-03-01    |
| `COLORADO_AI_ACT`  | `US-CO`      | true      | 2026-06-01    |

### Time / locale sanity (exactly 2 checks)

| name                    | evidence          |
|-------------------------|-------------------|
| `current_time_example`  | `4:58 Helsinki`   |
| `locale_lookup_ok`      | `auto-detect ok`  |

### σ_locale

```
σ_locale = 1 − (languages_ok + cultural_ok +
                legal_ok + time_ok) /
               (10 + 3 + 3 + 2)
```

v0 requires `σ_locale == 0.0`.

## Merge-gate contract

`bash benchmarks/v257/check_v257_locale_multilingual.sh`

- self-test PASSES
- 10 locales in canonical order, every `locale_ok`,
  `currency` is an ISO-4217 3-letter code
- 3 cultural styles in canonical order, every
  `example_locale` is one of the 10 declared
- 3 legal regimes in canonical order, every `compliant
  AND last_reviewed > 0`
- 2 time checks, every `pass`
- `σ_locale ∈ [0, 1]` AND `σ_locale == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed locale / culture / law /
  time manifest with FNV-1a chain.
- **v257.1 (named, not implemented)** — live locale
  auto-detection via OS + IANA tz db, on-disk MO/PO
  files for every listed locale, v202 culture-driven
  reply style, v199 live law-module lookup per
  jurisdiction, σ_compliance updated from a legal-source
  audit.

## Honest claims

- **Is:** a typed, falsifiable locale manifest where
  every row, style, regime, and time check is audited
  by the merge-gate.
- **Is not:** a live i18n pipeline.  v257.1 is where the
  manifest drives real user-facing translations and legal
  compliance checks.

---

*Spektre Labs · Creation OS · 2026*
