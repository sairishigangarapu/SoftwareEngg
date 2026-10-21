# UATC-031: CLI compress/extract, Compression Manager, error codes and demo runbook

**Sprint**: Sprint 8 (Integration and Demo)  
**Assignee**: Sai Rishi (with Manasa K for the CLI surface)  
**Priority**: P1  
**Requirements**: FR-01, FR-02, FR-03, FR-11, FR-12, FR-15  
**GitHub**: #44

## Description

Make the prototype demo-able end to end and give the team a repeatable dry
run to show the project guide.

## Delivered (PR #43)

- [x] `CompressionManager` in the core owns the compress/extract orchestration (SAD 3.4), so the pipeline is testable instead of sitting inside the CLI
- [x] Shared error codes and CLI exit codes from SAD 4.3.4, carried by a `UatcError`
- [x] CLI `compress` and `extract` sub-commands, plus `--block-size` and `--quiet`
- [x] `tests/core/compressor_test.cpp` covering compress, inspect, verify, extract and the error table
- [x] `scripts/demo.sh` - a one-command dry run: build, tests, corpus, compress/inspect, byte-for-byte round trip, corruption detection, benchmark
- [x] `docs/DEMO.md` - the runbook: what to run, what each step proves, what to say, and the remaining gaps

## Bugs found and fixed

- [x] The compressibility probe used a one-byte margin on its size estimate. On random data, chance runs of three identical bytes made the estimate come out two bytes *under* the original size, so incompressible data was pushed through the pipeline. The probe is now entropy-driven (7.5 bits per byte), with a size-based fallback for blocks too small for the entropy estimate to mean anything. Regression tests added.
- [x] The archive inspector had its own hard-coded transform whitelist and name table, so `info` failed with "Unsupported transform" on any archive using XOR or ByteShuffle. Both the inspector and the CLI now take their names and their candidate set from the core.
- [x] The CLI's transform name table was missing XOR and ByteShuffle.

## Still open, tracked separately

- #40 thread pool, FR-10 parallel execution
- #41 CRC32 and SHA-256 instead of FNV-1a, per the SAD
- #42 path traversal checks on extraction

## Follow-ups this issue does not cover

- [ ] `uatc benchmark` sub-command in the CLI. The spec in `docs/cli-spec.md` lists it; today it is only the pcomp harness.
- [ ] `uatc compress` on a directory, and multi-file archives. The reader currently handles one file per archive.
- [ ] `--workers N` option, once the thread pool lands.
- [ ] A `Jenkinsfile` and a `Dockerfile`, which the course tooling list requires.

## Acceptance Criteria

- `./scripts/demo.sh` runs green from a clean checkout
- Every extracted file is byte-for-byte identical to the original
- Corruption is detected and reported with the right error code and exit status
