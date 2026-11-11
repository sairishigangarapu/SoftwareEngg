# UATC - System Validation Report (Test Summary Report)

**Owner**: Manasa K (QA Lead; STP section 9 "produce the Test Summary Report")
**Build validated**: PR #59 (API security, folder archives, web UI) plus the
QA branch (`manasa/qa-tests`) and the fixes listed in section 5.
**Test environment**: Ubuntu 24.04, x86-64, 4 CPUs, GCC 13 (C++17),
headless Chromium 153 for the UI suite.
**Inputs**: SRS v1.0, SAD v1.0, STP v1.0. Traceability: `docs/rtm.md`.
Unit results and coverage: `docs/test-report.md`.

## 1. Summary

| | |
|---|---|
| STP test cases | 49 |
| Pass | 38 |
| Partial (passes, but a sub-check is missing or blocked) | 8 |
| Not run in this cycle | 3 (TC-NFR-03 fuzzing, TC-NFR-05 dummy transform, TC-NFR-06 second OS) |
| Fail | 0 |
| Defects found in this cycle | 6 (4 fixed, 2 open) |
| Core coverage | 91.0% lines, 81.1% branches (target 80 / 70) |
| Requirements with a passing test | 21 of 23 |

**Recommendation**: the system is fit for the final demonstration. It is
not yet at the STP exit criteria: three planned cases have not been run and
two defects are open (section 6).

## 2. How the results were produced

| Level (STP 5) | How | Command |
|---|---|---|
| Unit, integration | 32 C++ suites | `make test` |
| API (integration, security) | 80 checks over real HTTP | `tests/server/api_test.cpp` (part of `make test`) |
| System, performance, security | generated corpus, release CLI, one PASS/FAIL line per check | `scripts/system_validation.sh` |
| System (UI) | 17 Selenium tests against `uatc serve` | `make ui-test` |
| Coverage | gcov + gcovr, thresholds enforced | `make coverage` |
| Static analysis | SonarCloud quality gate on every PR | automatic |

`scripts/system_validation.sh` generates one file for every data category
in STP section 6: text (book, server log, source code), numeric (CSV time
series, int32 ramp, float64, sorted IDs), structured binary records,
already-compressed (gzip, random), synthetic (zeros, repeating pattern,
mixed), size edge cases (0 B, 1 B, block size -1 / exact / +1), and a folder
tree of 203 files (nested folders, spaces, Unicode names, 200 tiny files).

## 3. Results per STP test case

| ID | Req. | Status | Evidence |
|---|---|---|---|
| TC-FS-01 | FR-01 | Pass | `filesystem_test`; every single-file case in the validation run |
| TC-FS-02 | FR-01 | **Partial** | Nested folders and Unicode names restored identically (203 files). **Empty folders are not restored** (D-04) |
| TC-FS-03 | FR-01, FR-12 | **Partial** | Missing input -> E_NOT_FOUND (CLI, API, UI), no archive left. Unreadable input not tested (needs a non-root run) |
| TC-ARC-01 | FR-02 | Pass | `archive_writer_test`, `archive_reader_test`, `archive_inspector_test` |
| TC-ARC-02 | FR-03, NFR-01 | Pass | `integration_roundtrip_test`; 18 files in the validation run |
| TC-ARC-03 | FR-03, NFR-01 | Pass | 203-file tree identical (`diff -r`); `archive_service_test`; TC-UI-02 SHA-256 check |
| TC-ARC-04 | FR-03, NFR-01 | Pass | 0 B, 1 B, 1 MiB -1 / exact / +1, 200 tiny files: all identical |
| TC-CORE-01 | FR-04 | Pass | `block_slice_test` (BlockStats on known data) |
| TC-CORE-02 | FR-05 | **Partial** | System level correct: random and gzip data stored (ratio 1.00), zeros compressed 539x. Unit assertion `probe_test.cpp:122` fails with asserts on (D-03) |
| TC-CORE-03 | FR-06, NFR-01 | **Partial** | Every transform round-trips on all 18 corpus files. Unit assertion `transforms_extended_test.cpp:123` (a cost expectation, not a round trip) fails with asserts on (D-03) |
| TC-CORE-04 | FR-06 | **Partial** | `selector_test.cpp:23` expects Delta for a numeric ramp; the engine picks Byte-shuffle (D-03). Ties to the lowest ID are covered by the selector code path (ADR-05) |
| TC-CORE-05 | FR-07, NFR-01 | **Partial** | Huffman and baseline coder round-trip on every corpus file. Unit size expectation `huffman_test.cpp:70` fails with asserts on (D-03) |
| TC-CORE-06 | FR-08 | Pass | `compressor_test`; random 1 MB stored, archive 198 B larger (metadata only) |
| TC-CORE-07 | FR-08 | Pass | Already-gzipped file stored as-is (ratio 1.00, +198 B) |
| TC-CORE-08 | FR-06, FR-08 | Pass | Mixed file (numeric + zeros + random), 256 KiB blocks: 3 blocks Byte-shuffle, 3 stored; round trip identical |
| TC-INT-01 | FR-09, NFR-03 | Pass | One payload byte flipped: `verify` and `extract` both exit 13 E_BLOCK_CHECKSUM, nothing restored |
| TC-INT-02 | FR-09, NFR-03 | Pass | Archive checksum region changed: exit 14 E_ARCHIVE_CHECKSUM |
| TC-ERR-01 | FR-12 | Pass | Truncated at 10, 60, 200, 1000 bytes: exit 10 E_MALFORMED_ARCHIVE each, no crash or hang |
| TC-ERR-02 | FR-12 | Pass | Version field set to 99: exit 11 E_UNSUPPORTED_VERSION |
| TC-ERR-03 | FR-12 | Pass | A text file: exit 10 E_MALFORMED_ARCHIVE ("Invalid UATC archive magic") |
| TC-ERR-04 | FR-12 | Pass | Distinct exit codes 10/11/13/14/15 observed; UI shows a distinct sentence per code (TC-UI-04) |
| TC-PAR-01 | FR-10 | Pass | 24.5 MB input: workers=1 and five runs of workers=8 give the same SHA-256 |
| TC-PAR-02 | FR-10, NFR-03 | Pass | Repeated runs identical (above, plus `parallel_test`); ThreadSanitizer clean for the pool, the pipeline and the API (`docs/quality-report.md`, `docs/test-report.md`). The STP asks for 20 repetitions; 5 were run here |
| TC-PAR-03 | FR-10 | Pass | `thread_pool_test` |
| TC-PAR-04 | NFR-02 | **Partial** (recorded) | See 4.1. Input 24.5 MB instead of the STP's 256 MiB |
| TC-PAR-05 | NFR-02 | Pass | `parallel_test`: serial path below 1 MiB |
| TC-INFO-01 | FR-11 | Pass | CLI, API and UI; folder archive lists all files; nothing extracted |
| TC-VER-01 | FR-09, FR-11 | Pass | Exit 0, "OK: n file(s), m block(s) verified", nothing written |
| TC-CLI-01 | FR-01, FR-03, FR-11 | Pass | `compress`, `extract`, `info`, `verify`, `benchmark`, `serve`, `--help` all present; exit codes as SAD 4.3.4 |
| TC-API-01..04 | FR-15, FR-14 | Pass | `api_test` (80 checks) |
| TC-UI-01..05 | FR-13, FR-14, NFR-04, NFR-08 | Pass | `tests/ui/test_ui.py`, 17 tests, 3 consecutive green runs |
| TC-NFR-01 | NFR-01 | Pass | Every corpus file and the 203-file tree verified and restored with identical SHA-256 |
| TC-NFR-02 | NFR-02 | Pass (recorded) | See 4.2. Comparison repeated: ratios are deterministic |
| TC-NFR-03 | NFR-03 | **Not run** | No fuzzer yet (planned: 10,000 mutated archives under ASan/UBSan) |
| TC-NFR-05 | NFR-05 | **Not run** | No test-only dummy transform yet |
| TC-NFR-06 | NFR-06 | **Not run** | Only Ubuntu 24.04 was used this cycle; the Docker build and a second OS (Windows, used by the team) are still to be run |
| TC-NFR-07 | NFR-07 | Pass | Ratio, time, throughput and per-transform block counts reported by CLI, API and UI |
| TC-SEC-01 | FR-03, FR-12 | Pass | Archive with `../evil.bin` **and a recomputed archive checksum**: `verify` and `extract` exit 15 E_PATH_TRAVERSAL; nothing written inside or outside the destination |
| TC-SEC-02 | FR-12, NFR-03 | Pass | block_count and file_count set to 4,294,967,295: rejected in 0.01 s under a 256 MB memory cap |
| TC-SEC-03 | FR-15 | Pass | Missing/wrong token, foreign Origin, non-loopback Host -> 403 (`api_test`) |
| TC-SEC-04 | FR-15 | Pass | `..`, encoded `..`, double encoding, NUL, symlink escape, absurd numbers -> refused (`api_test`) |
| TC-SEC-05 | NFR-03 | **Partial** | SonarCloud quality gate enforced on PRs (all findings on PR #59 fixed). Dependency check not set up |

## 4. Performance and comparison (recorded, not pass/failed)

The SRS and STP state that UATC does not aim to beat gzip or Zstandard;
the numbers are reported as measured.

### 4.1 Throughput by worker count (TC-PAR-04)

Input: 24.5 MB CSV-like text, compression with the default 1 MiB blocks,
4 CPUs.

| Workers | Seconds | MB/s | Speed-up |
|---|---|---|---|
| 1 | 2.04 | 12.0 | 1.0x |
| 2 | 1.06 | 23.1 | 1.9x |
| 4 | 0.69 | 35.5 | 2.9x |
| 8 | 0.72 | 34.2 | 2.8x |

Scaling is close to linear up to the 4 available cores and flat beyond, as
expected; the archive is identical for every worker count (TC-PAR-01).

### 4.2 Ratio against gzip -6 and zstd -3 (TC-NFR-02)

Ratio = original / compressed (higher is better).

| File | UATC | gzip -6 | zstd -3 |
|---|---|---|---|
| text/book.txt | 1.94 | 6.30 | 5.26 |
| text/server.log | 1.75 | 8.41 | 10.01 |
| numeric/timeseries.csv | 2.23 | 2.99 | 5.18 |
| numeric/int32_ramp.bin | **3.53** | 2.88 | 1.46 |
| structured/records.db | 1.79 | 5.58 | 7.94 |
| compressed/random.bin | 1.00 | 1.00 | 1.00 |
| synthetic/zeros.bin | 539 | 1009 | 24390 |

Findings:
- On **numeric data with a regular structure** (int32 ramp) UATC's
  per-block transforms beat both general-purpose tools.
- On **text and logs** UATC is 3-5x behind: UATC has no LZ-style match
  finding (repeated phrases), which is where gzip and zstd gain most. This
  matches the SRS positioning and is a candidate for future work.
- **Incompressible data** is stored, never expanded beyond the fixed
  metadata (about 200 bytes per file).
- float64 sine data was stored as-is (ratio 1.00): no current transform
  helps on floating-point mantissas. A bit-plane transform (SAD ID 5,
  reserved) would target this.

## 5. Defects found in this cycle

| ID | Severity | Found by | Description | Status |
|---|---|---|---|---|
| D-01 | Medium | TC-UI-03, TC-UI-05 | Typing a source and then clicking the destination inserted a suggested destination, so the typed text was appended to it and the job failed | **Fixed** (suggest only from Browse) |
| D-02 | Low | coverage run | gcov reported negative branch counts in multi-threaded code | **Fixed** (`-fprofile-update=atomic`) |
| D-03 | High (process) | CMake run | The Makefile builds tests with `-DNDEBUG`, which removes every `assert()`. With asserts on, 7 core suites fail: `transform_test.cpp:75`, `transforms_extended_test.cpp:123`, `probe_test.cpp:122`, `selector_test.cpp:23`, `planner_test.cpp:79`, `path_safety_test.cpp:150`, `huffman_test.cpp:70`. Most are expectations that predate later engine changes (exact cost model, Huffman, Byte-shuffle wins). `path_safety_test.cpp:150` is a broken test: it reads the file-table offset at byte 12 instead of 16, so its tampering never lands. The path rule itself was verified separately (TC-SEC-01) | **Open**, assigned to the core owner |
| D-04 | Medium | TC-FS-02 | Empty folders are not stored in folder archives (the enumerator only records regular files), so they are missing after extraction | **Open**, archive layer owner |
| D-05 | Low | TC-INT-01 via CLI | `uatc extract` exited 10 (E_MALFORMED_ARCHIVE) for any failure, including checksum mismatches that `verify` reports as 13/14 | **Fixed** (the CLI uses the shared classifier) |
| D-06 | Medium (security) | TC-SEC-01 | `uatc verify` reported OK for an archive with a `../` path and a valid checksum (extraction was already refused) | **Fixed** (the verifier checks every recorded path; regression test added) |

## 6. Exit criteria (STP section 5)

| Criterion | Status |
|---|---|
| 100% of planned test cases executed | **Not met**: 46 of 49 (TC-NFR-03, TC-NFR-05, TC-NFR-06 not run) |
| No open critical or high defects | **Not met**: D-03 is open (High: unit assertions are not enforced) |
| Whole corpus round-trips with identical SHA-256 | Met |
| Unit coverage >= 80% lines and >= 70% branches on core modules | Met (91.0% / 81.1%) |
| SonarQube quality gate passes; acceptance criteria satisfied | Met for PR #59 after fixes |

### Work needed to reach the exit criteria

1. **D-03**: build tests without `-DNDEBUG` and update or fix the seven
   assertions (core owner). Add a CTest stage to Jenkins so this cannot
   recur.
2. **D-04**: record empty directories in the file table (archive owner),
   with a TC-FS-02 test.
3. **TC-NFR-03**: a mutation fuzzer over the archive reader (10,000+
   cases, ASan/UBSan).
4. **TC-NFR-05**: a test-only transform registered with a new ID, compressed
   and extracted without touching the archive, API, UI or CLI code.
5. **TC-NFR-06**: run `make test` in the Docker image and on Windows.

## 7. Sign-off

| Role | Name | Date |
|---|---|---|
| QA Lead | Manasa K | |
| Dev Lead / Product Owner | Sai Rishi Gangarapu | |
| Project Guide | Dr. Sapna V M | |
