# UATC - Requirements Traceability Matrix (RTM)

**Owner**: Manasa K (QA Lead; STP section 9: "maintain this plan and the
traceability matrix")
**Template**: SRS Appendix C (course SRS template), extended with the STP's
test case IDs (STP section 13) and the status from
`docs/validation-report.md`.
**Sources**: SRS v1.0 section 3 (FR-01..FR-15) and 3.3 (non-functional
requirements, labelled NFR-01..NFR-08 by the STP); SAD v1.0 sections 3.4,
3.8, 4.x; STP v1.0 Appendix A.

Columns:
- **Architecture reference**: SAD 3.4 component.
- **Design reference**: SAD section.
- **Code**: the files that implement the requirement.
- **Test case ID**: unit / integration cases (STP levels Unit, Integration).
- **System test case ID**: system, performance and security cases.
- **Status**: from the validation run (Pass / Partial / Not run); see
  `docs/validation-report.md` for the evidence behind each.

## Functional requirements

| Sl. | Req. ID | Brief description | Architecture ref. | Design ref. | Code | Test case ID | System test case ID | Status |
|---|---|---|---|---|---|---|---|---|
| 1 | FR-01 | Accept a file or directory and enumerate its content | File Analyzer | SAD 4.1, 4.2 | `src/filesystem/file_enumerator.cpp`, `src/filesystem/file_reader.cpp`, `src/core/archive_service.cpp` | TC-FS-01, TC-FS-02, TC-FS-03 | TC-CLI-01, TC-UI-01 | Partial (empty folders not kept, D-04) |
| 2 | FR-02 | Create a .uatc archive (metadata, file table, block index, payloads) | Archive Builder, Block Manager | SAD 4.1 (archive format) | `src/archive/archive_writer.cpp`, `src/archive/multi_file.cpp`, `src/archive/archive_binary.cpp`, `src/archive/block.cpp` | TC-ARC-01 | TC-CLI-01, TC-UI-01 | Pass |
| 3 | FR-03 | Extract with byte-for-byte fidelity | Archive Reader, Compression Manager | SAD 4.2 (Figure 4) | `src/archive/archive_reader.cpp`, `src/archive/multi_file.cpp`, `src/core/compressor.cpp`, `src/core/archive_service.cpp`, `src/core/path_safety.cpp` | TC-ARC-02, TC-ARC-04 | TC-ARC-03, TC-UI-02, TC-NFR-01, TC-SEC-01 | Pass |
| 4 | FR-04 | Per-block statistics | File Analyzer, Probe | SAD 4.1 | `src/core/block_stats.cpp`, `src/core/block_slicer.cpp` | TC-CORE-01 | - | Pass |
| 5 | FR-05 | Compressibility probe before full work | Compressibility Probe | SAD 4.1 | `src/core/compressibility_probe.cpp`, `src/core/planner.cpp` | TC-CORE-02 | TC-NFR-01 | Partial (unit assertion fails with asserts on, D-03) |
| 6 | FR-06 | Lowest-cost transform per block | Adaptive Transform Engine, Transform Registry | SAD 4.1, 4.3.2 | `src/core/transform_selector.cpp`, `src/core/cost_estimator.cpp`, `src/core/{delta,xor,byte_shuffle,rle}_transform.cpp`, `src/core/transform_factory.cpp`, `include/core/registry.hpp` | TC-CORE-03, TC-CORE-04, TC-CORE-08 | TC-NFR-01 | Partial (D-03) |
| 7 | FR-07 | Lossless entropy coding | Entropy Coding Engine | SAD 4.3.2 | `src/core/huffman_coder.cpp`, `src/core/baseline_entropy_coder.cpp`, `src/archive/codec.cpp` | TC-CORE-05 | TC-NFR-01 | Partial (D-03) |
| 8 | FR-08 | Store the block when coding does not make it smaller | Transform Engine, Compression Manager | SAD 4.1 | `src/core/planner.cpp`, `src/core/transform_selector.cpp`, `src/core/compressor.cpp` | TC-CORE-06, TC-CORE-07, TC-CORE-08 | TC-NFR-01 | Pass |
| 9 | FR-09 | Block and archive checksums checked; corruption reported | Integrity Module | SAD 4.2 (Figure 4), 4.4 | `src/core/crc32.cpp`, `src/core/sha256.cpp`, `src/archive/archive_reader.cpp`, `src/archive/multi_file.cpp`, `src/archive/archive_verifier.cpp` | TC-INT-01, TC-INT-02 | TC-VER-01, TC-UI-04 | Pass |
| 10 | FR-10 | Parallel blocks, identical archive for any worker count | Thread-Pool Scheduler | SAD 4.1, 4.2 (Figure 3), ADR-05 | `src/core/thread_pool.cpp`, `src/core/compressor.cpp`, `src/core/archive_service.cpp` | TC-PAR-03 | TC-PAR-01, TC-PAR-02, TC-PAR-04, TC-PAR-05 | Pass |
| 11 | FR-11 | Archive information without extraction | Archive Inspector | SAD 4.3.1 | `src/archive/archive_inspector.cpp`, `src/cli/main.cpp` (`info`), `src/server/api_server.cpp` (`/api/archives/info`) | TC-INFO-01 | TC-CLI-01, TC-INFO-01 (UI) | Pass |
| 12 | FR-12 | Distinct errors: malformed, missing block, checksum, version | Archive Reader, Integrity Module | SAD 4.3.4 | `src/core/error.cpp`, `src/archive/archive_reader.cpp`, `src/archive/archive_inspector.cpp`, `src/archive/archive_verifier.cpp` | TC-ERR-01, TC-ERR-02, TC-ERR-03, TC-SEC-02 | TC-ERR-04, TC-SEC-01 | Pass |
| 13 | FR-13 | Browser interface to select and start operations | Web Frontend | SAD 4.5 | `web/index.html`, `web/app.js`, `web/styles.css` | - | TC-UI-01, TC-UI-02 | Pass |
| 14 | FR-14 | Progress, status, errors and statistics in the UI | Web Frontend, Job Manager | SAD 4.2 (Figure 3), 4.5 | `web/app.js`, `src/server/api_server.cpp` (JobManager), `include/core/progress.hpp` | TC-API-03 | TC-UI-03, TC-UI-04 | Pass |
| 15 | FR-15 | HTTP API between the frontend and the C++ backend | API / Application Layer | SAD 4.3.1, 3.9 | `src/server/api_server.cpp`, `src/cli/main.cpp` (`serve`), `web/app.js` (`api()`) | TC-API-01, TC-API-02, TC-API-03, TC-API-04 | TC-SEC-03, TC-SEC-04 | Pass |

## Non-functional requirements

| Sl. | Req. ID | Brief description | Architecture ref. | Design ref. | Code | Test case ID | System test case ID | Status |
|---|---|---|---|---|---|---|---|---|
| 16 | NFR-01 | Correctness: byte-for-byte reconstruction | all layers | SAD 3.1 | whole pipeline | TC-ARC-02, TC-ARC-04, TC-CORE-03, TC-CORE-05 | TC-NFR-01 | Pass |
| 17 | NFR-02 | Performance: scales with threads; honest comparison with gzip/zstd | Thread-Pool Scheduler, Benchmark Harness | SAD 3.6 | `src/core/thread_pool.cpp`, `src/cli/main.cpp` (`benchmark`), `pcomp/`, `scripts/system_validation.sh` | - | TC-PAR-04, TC-PAR-05, TC-NFR-02 | Recorded (see report) |
| 18 | NFR-03 | Reliability: corruption always detected | Integrity Module | SAD 3.9 | as FR-09, FR-12 | TC-INT-01, TC-INT-02, TC-SEC-02 | TC-PAR-02, TC-NFR-03, TC-SEC-05 | Pass |
| 19 | NFR-04 | Usability: simple CLI and web UI | Web Frontend, CLI | SAD 4.5 | `web/*`, `src/cli/main.cpp` | - | TC-UI-04 (+ keyboard and label tests) | Pass |
| 20 | NFR-05 | Maintainability: add a transform without redesign | Transform interface, Registry | SAD 4.3.2 | `include/core/transform.hpp`, `include/core/registry.hpp`, `src/core/transform_factory.cpp` | TC-NFR-05 | - | Not run |
| 21 | NFR-06 | Portability: standard C++17 | - | SAD 3.6 | `CMakeLists.txt`, `Makefile`, `Dockerfile` | - | TC-NFR-06 | Not run (one OS only) |
| 22 | NFR-07 | Observability: ratio, time, throughput, per-block transform | Compression Manager, Job Manager | SAD 4.4 | `src/core/archive_service.cpp`, `src/server/api_server.cpp` (`job_json`), `src/cli/main.cpp` | TC-NFR-07 | TC-UI-03 | Pass |
| 23 | NFR-08 | Responsiveness: UI usable during long jobs | Web Frontend, Job Manager | SAD 4.5, ADR-07 | `web/app.js` (`poll`), `src/server/api_server.cpp` | - | TC-UI-05 | Pass |

## Test case -> test file

Where each STP case is automated, so a failing requirement can be traced to
the exact test.

| Test case IDs | Automated in |
|---|---|
| TC-FS-01, TC-FS-02 | `tests/filesystem_test.cpp`, `tests/core/multi_file_test.cpp`, `scripts/system_validation.sh` |
| TC-FS-03 | `tests/core/compressor_test.cpp`, `tests/server/api_test.cpp` |
| TC-ARC-01 | `tests/archive_writer_test.cpp`, `tests/archive_reader_test.cpp`, `tests/archive_inspector_test.cpp` |
| TC-ARC-02 | `tests/integration_roundtrip_test.cpp` |
| TC-ARC-03 | `tests/core/archive_service_test.cpp`, `tests/core/multi_file_test.cpp`, `scripts/system_validation.sh` |
| TC-ARC-04 | `tests/block_test.cpp`, `tests/core/block_slice_test.cpp`, `scripts/system_validation.sh` |
| TC-CORE-01 | `tests/core/block_slice_test.cpp` |
| TC-CORE-02 | `tests/core/probe_test.cpp` |
| TC-CORE-03 | `tests/core/transform_test.cpp`, `tests/core/transforms_extended_test.cpp` |
| TC-CORE-04, TC-CORE-08 | `tests/core/selector_test.cpp` |
| TC-CORE-05 | `tests/core/codec_test.cpp`, `tests/core/huffman_test.cpp` |
| TC-CORE-06, TC-CORE-07 | `tests/core/compressor_test.cpp`, `tests/core/planner_test.cpp` |
| TC-INT-01, TC-INT-02 | `tests/core/integrity_test.cpp`, `tests/archive_verifier_test.cpp`, `tests/core/archive_service_test.cpp`, `scripts/system_validation.sh` |
| TC-ERR-01..03 | `tests/archive_reader_test.cpp`, `tests/core/archive_service_test.cpp`, `scripts/system_validation.sh` |
| TC-ERR-04 | `tests/core/compressor_test.cpp` (exit-code table), `tests/ui/test_ui.py` (TC-UI-04) |
| TC-PAR-01, TC-PAR-05 | `tests/core/parallel_test.cpp`, `tests/core/archive_service_test.cpp`, `scripts/system_validation.sh` |
| TC-PAR-02, TC-PAR-03 | `tests/core/thread_pool_test.cpp`, `tests/core/parallel_test.cpp` (ThreadSanitizer: `docs/quality-report.md`) |
| TC-PAR-04, TC-NFR-02 | `scripts/system_validation.sh`, `uatc benchmark --gzip` |
| TC-INFO-01, TC-VER-01 | `tests/archive_inspector_test.cpp`, `tests/archive_verifier_test.cpp`, `tests/core/archive_service_test.cpp`, `tests/ui/test_ui.py` |
| TC-CLI-01 | `scripts/system_validation.sh`, `scripts/demo.sh` |
| TC-API-01..04 | `tests/server/api_test.cpp` |
| TC-UI-01..05 | `tests/ui/test_ui.py` |
| TC-NFR-01, TC-NFR-07 | `scripts/system_validation.sh` |
| TC-SEC-01 | `tests/core/path_safety_test.cpp`, `tests/core/archive_service_test.cpp`, `scripts/system_validation.sh` |
| TC-SEC-02 | `scripts/system_validation.sh`, `tests/fuzz/archive_fuzz_test.cpp` |
| TC-NFR-03 | `tests/fuzz/archive_fuzz_test.cpp` (`make fuzz` under ASan/UBSan) |
| TC-SEC-03, TC-SEC-04 | `tests/server/api_test.cpp` |
| TC-SEC-05 | SonarCloud quality gate on every pull request |
| TC-NFR-05, TC-NFR-06 | not automated yet (see the validation report) |

## Coverage of the matrix

- Every FR-01..FR-15 and NFR-01..NFR-08 has at least one test case.
- Every requirement except NFR-05 and NFR-06 has at least one test that was
  run in this cycle.
- Requirements with a status other than Pass have their open defect or
  missing run named in `docs/validation-report.md`, sections 3 and 5.
