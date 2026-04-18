# v169 σ-Ontology — automatic knowledge-graph + OWL-lite builder

## Problem

v135 Prolog reasons over **hand-written** ground facts. For an
LLM-adjacent system those facts never keep up with the corpus
and the session: every new entity (`lauri`, `creation_os`, `σ`,
`v101`, `rpi5`, …) needs a human to file it, which doesn’t
scale.

## σ-innovation

v169 **auto-constructs** a knowledge graph and an OWL-lite
schema from the user’s v115 memory entries and the v152 corpus,
σ-gating every extraction so only trusted triples land in the
KG.

Pipeline:

1. **Entity–relation extraction.** Each memory entry is parsed
   into ≤ 4 candidate `(subject, predicate, object)` triples
   with a `σ_extraction` per triple.
2. **τ-gate.** `τ_extract = 0.35`; σ above is dropped. The
   merge-gate checks that the gate is **non-trivial** (some
   triples kept, some dropped).
3. **Hierarchical typing.** Kept triples feed a classifier that
   assigns each entity to one of 6 top-level classes
   (`Person`, `Software`, `Metric`, `Kernel`, `Device`,
   `Concept`) with a `σ_type` confidence.
4. **OWL-lite schema.** The typed universe is emitted as OWL-
   lite-JSON so downstream kernels can traverse the schema in
   v0 without a real reasoner. v169.1 writes `~/.creation-os/
   ontology.owl` as real OWL/XML.
5. **Corpus navigation.** A query API returns source entry-ids
   for any `(predicate, object?)` pattern, so v152-style
   questions (“which entries mention σ?”) resolve
   **structurally** instead of by substring search.

## Merge-gate

`make check-v169` runs
`benchmarks/v169/check_v169_ontology_rdf_extract.sh` and
verifies:

- 50-entry fixture yields ≥ 50 candidate triples
- τ-gate drops *some* triples but keeps *some* (non-trivial κ)
- every triple has `σ ∈ [0,1]` and a valid source id `0..49`
- `lauri → Person`, `creation_os → Software`, `σ → Metric`,
  some `Kernel` and `Device` entities exist
- query `mentions σ` returns ≥ 1 entry id
- OWL-lite-JSON has exactly 6 classes and no entity is listed
  in more than one class
- two full runs produce byte-identical JSON output
  (determinism)

## v169.0 (shipped) vs v169.1 (named)

| | v169.0 | v169.1 |
|-|-|-|
| Source | baked 50-entry fixture | real v115 memory + v152 corpus |
| Extractor | deterministic `sscanf` + σ jitter | BitNet-driven extraction |
| Schema | OWL-lite-JSON | OWL/XML at `~/.creation-os/ontology.owl` |
| Update | rebuild from scratch | incremental merge (new facts extend, never rewrite) |
| Reasoner | standalone query API | v135 Prolog consumes the KG |

## Honest claims

- **Is:** a deterministic, offline-safe KG builder and OWL-lite
  emitter that demonstrates σ-gated extraction, structural
  corpus navigation and six-class typing on a synthetic
  fixture.
- **Is not:** a production NLP extractor. v0 parses the fixture
  with `sscanf`; BitNet-driven extraction and real OWL output
  are the v169.1 work items.
