# UATC Sprint Plan — Milestone Based

**Author**: Sai Rishi Gangarapu (Team Lead)  
**Branch**: `Rishi_implementation` (Days 1-5), `Rishi_implementation_v2` (Days 6-10)  
**Scope**: Compression core + adaptive transform engine (FR-04, FR-05, FR-06, FR-07)  
**Shape**: two sprints, one per milestone, five days each

## Milestones

| Milestone | Days | SRS phase | Goal | Status |
|-----------|------|-----------|------|--------|
| **1 — MVP Compressor** | 1-5 | Phase 2 | A working lossless compressor: any file in, the same bytes out | Complete |
| **2 — Adaptive Engine** | 6-10 | Phase 3 + 6 | The full transform set with automatic per-block selection, and a demoable prototype | Complete, with three items carried forward |

Each sprint is a milestone with a **demonstrable outcome**, not a set of tasks.
A sprint is not done until the milestone it stands for can be shown working.

Sampada's `pcomp` scaffold (Days 1-5 of the pcomp workplan) ran in parallel
and provides the benchmarking half of the execution layer, FR-10.

---

# Sprint 1 — Milestone 1: MVP Compressor (Days 1-5)

**Goal**: build a lossless compressor that turns any file into an archive and
back again, byte for byte, with the data checked at every step.

**Milestone demo**: compress a file, extract it, and prove the two are
identical. The archive is versioned and checksummed, and the file is split
into blocks so that later work can work on it in parallel.

**Exit criteria**

- [x] A file can be split into fixed-size blocks and reassembled exactly
- [x] Block statistics are computed and available to the compressor
- [x] A transform interface exists and the first transform round-trips
- [x] A baseline entropy coder is behind a `Codec` interface and round-trips
- [x] The archive is written and read back, and the result is byte-identical
- [x] Every block carries a checksum, and a corrupted archive is rejected
- [x] The transform ids and codec ids match the SAD
- [x] A test suite runs for every module

**Issues**

| Day | Issue | Title | Status |
|-----|-------|-------|--------|
| 1 | UATC-001 | Core skeleton and Transform interface | Done |
| 1 | UATC-002 | Pipeline documentation | Done |
| 1 | UATC-003 | Unit test target | Done |
| 2 | UATC-004 | Finalise BlockStats structures | Done |
| 2 | UATC-005 | Configurable block size | Done |
| 2 | UATC-006 | Metadata review with Chakshu | Done |
| 3 | UATC-007 | Block slicing | Done |
| 3 | UATC-008 | BlockStats computation | Done |
| 3 | UATC-009 | Edge case tests | Done |
| 4 | UATC-010 | NoneTransform | Done |
| 4 | UATC-011 | TransformResult | Done |
| 4 | UATC-012 | Round-trip tests | Done |
| 4 | UATC-013 | TransformRegistry | Done |
| 5 | UATC-014 | Entropy coder integration | Done |
| 5 | UATC-015 | CodecResult and codec ids | Done |
| 5 | UATC-016 | Entropy coder tests | Done |
| 5 | UATC-017 | Measure encoded size on the initial dataset | Done |

---

# Sprint 2 — Milestone 2: Adaptive Engine (Days 6-10)

**Goal**: complete the v1 transform set, let the engine choose per block on its
own, and make the whole thing demonstrable.

**Milestone demo**: compress a file that holds three different kinds of data,
and watch the engine pick a different transform for each block, store the
incompressible parts unchanged, and restore the file exactly.

**Exit criteria**

- [x] All five v1 transforms exist and round-trip
- [x] Transforms are added through a registry, with no change to the archive
      format, the CLI or the frontend
- [x] The compressibility probe rejects incompressible data before any work
- [x] The selector picks the cheapest transform, and stores the block when
      nothing beats leaving it alone
- [x] The transform ids match the SAD, and the archive layer accepts every
      transform the core registers
- [x] The CLI compresses and extracts, with before, during and after reporting
- [x] The demo script runs green end to end
- [ ] The thread pool exists and produces byte-identical archives for any
      worker count (FR-10) — **carried forward**, UATC-028
- [ ] The checksums are CRC32 and SHA-256 as the SAD specifies — **carried
      forward**, UATC-029
- [ ] The transform cost estimate is accurate enough to be shown as a number —
      **carried forward**, UATC-032

**Issues**

| Day | Issue | Title | Status |
|-----|-------|-------|--------|
| 6 | UATC-018 | DeltaTransform | Done |
| 6 | UATC-019 | XorTransform | Done |
| 7 | UATC-020 | ByteShuffleTransform | Done |
| 7 | UATC-021 | RleTransform | Done |
| 7 | UATC-022 | Transform factory | Done |
| 7 | UATC-023 | Shared cost estimator | Done |
| 8 | UATC-024 | CompressibilityProbe | Done |
| 9 | UATC-025 | TransformSelector | Done |
| 9 | UATC-026 | Core and archive integration test | Done |
| 9 | UATC-027 | Transform id alignment and Chakshu verification | Done |
| 10 | UATC-031 | CLI compress/extract, Compression Manager, error codes, demo runbook | Done |
| — | UATC-028 | Thread pool, FR-10 parallel execution | **Open** — Sampada |
| — | UATC-029 | CRC32 per block and SHA-256 per file and archive | **Open** — Chakshu + Rishi |
| — | UATC-030 | Path traversal checks on extraction | **Open** — Chakshu + Manasa |
| — | UATC-032 | The cost estimate is a poor size predictor | **Open** — Rishi |

---

# Where each milestone stands

## Milestone 1 — MVP Compressor: complete

The compressor is lossless, the archive is versioned and checksummed, and the
round trip is exact. 12 test suites cover it. The one thing this milestone
deliberately does not do is choose between transforms: it has exactly one, so
it compresses everything the same way.

## Milestone 2 — Adaptive Engine: complete, with three items open

The engine now has the full candidate set, probes before it works, picks the
cheapest transform per block, stores what does not benefit, and reports what
it is doing before, during and after. 21 test suites cover it.

The three carried-forward items are the ones that need someone else's time or
a change to the archive format, and they are tracked as issues rather than
left implicit:

- **UATC-028** — the thread pool. FR-10 is the last unimplemented functional
  requirement. Until it lands, the benchmark records every worker count above
  one as `skipped_parallel_not_implemented` rather than faking a measurement.
- **UATC-029** — the checksums are FNV-1a, not the CRC32 and SHA-256 the SAD
  specifies. This changes the archive format, so it needs the whole team.
- **UATC-032** — the cost estimate ranks transforms well but is a poor size
  predictor, and it is now shown to the user.

**UATC-030** is listed as open for completeness: the extraction path already
rejects absolute paths and `..` segments, but the equivalent check for the
paths that reach the HTTP API is still to come.

---

# Artefacts for each milestone

| Milestone | Artefact | Where |
|-----------|----------|-------|
| 1 | Core skeleton, block operations, first transform, baseline coder | `include/core/`, `src/core/` |
| 1 | Archive format, reader, writer, integrity | `include/archive/`, `src/archive/` |
| 1 | Test suite for every module | `tests/`, `tests/core/` |
| 2 | Full transform set, probe, selector, planner, progress | `include/core/`, `src/core/` |
| 2 | CLI with before, during and after reporting | `src/cli/main.cpp` |
| 2 | Demo runbook and script | `docs/DEMO.md`, `scripts/demo.sh` |
| 2 | Sprint and issue plan | `docs/sprints/` |
