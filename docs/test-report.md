# UATC - Unit Test and Coverage Report

**Owner**: Manasa K (QA Lead, STP section 9)
**Scope**: every automated C++ test suite and the Selenium UI suite, run on
the code of PR #59 plus this change.
**STP references**: section 5 (strategy, exit criteria), section 6 (tools),
section 14 (metrics), Appendix A (test cases).

## 1. How to reproduce

| What | Command | Output |
|---|---|---|
| All C++ unit and integration tests | `make all && make test` | `ALL TESTS PASSED` |
| Same suites through CTest | `cmake -S . -B build && cmake --build build && ctest --test-dir build` | per-suite pass/fail |
| Line and branch coverage | `scripts/coverage.sh` (needs `pip install gcovr`) | `build-cov/coverage/index.html`, `summary.txt`, `summary.json` |
| Selenium UI tests | `make cli && pip install -r tests/ui/requirements.txt && python3 -m pytest tests/ui -v` | 17 tests |
| Fuzz test under sanitizers (TC-NFR-03) | `make fuzz` | `PASS` lines and a verdict histogram |
| Thread-safety check (TC-PAR-02) | build `tests/server/api_test.cpp` with `-fsanitize=thread` | no reports |

## 2. Results summary

| Suite | Tests | Result |
|---|---|---|
| C++ unit and integration suites (`make test`) | 32 suites | all pass |
| API suite (`tests/server/api_test.cpp`) | 80 checks | all pass (one check, the read-only folder, runs only as a non-root user); clean under ThreadSanitizer |
| Folder-archive facade (`tests/core/archive_service_test.cpp`) | 29 checks | all pass |
| Selenium UI suite (`tests/ui/test_ui.py`) | 17 tests | all pass, 3 consecutive runs (no flakiness) |
| Archive-reader fuzz test (`make fuzz`) | 45,250 damaged archives | all handled correctly under AddressSanitizer + UBSan |

## 3. Coverage (gcov + gcovr)

Measured over every C++ suite. Branches generated only for exception
handling and branches the compiler proves unreachable are excluded (they
are not decisions in our code). The CLI's `main()` is excluded: it is
exercised by the demo and UI runs, not by unit tests.

### 3.1 Against the STP exit criterion

The STP target is **>= 80% lines and >= 70% branches on core modules**.
`scripts/coverage.sh` fails if the core falls below it.

| Layer | Lines | Branches | Target met |
|---|---|---|---|
| **src/core** (compression core) | **91.0%** (956/1051) | **81.1%** (765/943) | yes |
| src/archive | 86.5% (839/970) | 70.3% (613/872) | yes |
| src/server (HTTP API) | 81.5% (684/839) | 72.0% (788/1094) | yes |
| src/filesystem | 81.4% (35/43) | 61.5% (32/52) | lines only |
| **All of src/** | **86.6%** (2514/2903) | **74.3%** (2199/2961) | yes |

### 3.2 Per file

| File | Lines | Branches |
|---|---|---|
| src/core/archive_service.cpp | 93% | 86% |
| src/core/baseline_entropy_coder.cpp | 90% | 82% |
| src/core/block_slicer.cpp | 92% | 93% |
| src/core/block_stats.cpp | 100% | 90% |
| src/core/byte_shuffle_transform.cpp | 94% | 96% |
| src/core/compressibility_probe.cpp | 100% | 100% |
| src/core/compressor.cpp | 95% | 80% |
| src/core/cost_estimator.cpp | 100% | 100% |
| src/core/crc32.cpp | 100% | 90% |
| src/core/delta_transform.cpp | 94% | 100% |
| src/core/error.cpp | 55% | 54% |
| src/core/huffman_coder.cpp | 96% | 89% |
| src/core/path_safety.cpp | 100% | 69% |
| src/core/planner.cpp | 97% | 86% |
| src/core/rle_transform.cpp | 92% | 90% |
| src/core/sha256.cpp | 88% | 60% |
| src/core/thread_pool.cpp | 96% | 79% |
| src/core/transform_factory.cpp | 87% | 89% |
| src/core/transform_selector.cpp | 80% | 82% |
| src/core/xor_transform.cpp | 94% | 100% |
| src/archive/archive_binary.cpp | 85% | 55% |
| src/archive/archive_inspector.cpp | 82% | 64% |
| src/archive/archive_reader.cpp | 83% | 67% |
| src/archive/archive_verifier.cpp | 94% | 90% |
| src/archive/archive_writer.cpp | 91% | 81% |
| src/archive/block.cpp | 95% | 100% |
| src/archive/codec.cpp | 85% | 76% |
| src/archive/multi_file.cpp | 88% | 73% |
| src/archive/transforms.cpp | 100% | 60% |
| src/filesystem/file_enumerator.cpp | 86% | 78% |
| src/filesystem/file_reader.cpp | 76% | 51% |
| src/server/api_server.cpp | 81% | 72% |

### 3.3 Known gaps (next coverage work)

- **src/core/error.cpp (55%)**: the `error_code_name` / `error_exit_code`
  tables are switch statements; several codes (E_MISSING_BLOCK,
  E_ARCHIVE_CHECKSUM, E_INTERNAL) are never raised by a test. A table-driven
  test over every `ErrorCode` would close this.
- **Archive reader/inspector bounds checks (64-67% branches)**: each
  malformed-header case is its own branch. The fuzz test (TC-NFR-03) now
  drives them; it is not included in the coverage run yet.
- **src/filesystem (61.5% branches)**: permission-denied and unreadable-file
  paths need a non-root test run (root ignores permission bits).
- **sha256.cpp (60% branches)**: the multi-chunk streaming paths need inputs
  that cross the 64-byte block boundary in several feeds.

## 4. UI test cases (Selenium, `tests/ui/test_ui.py`)

The suite starts the real backend (`uatc serve`) with a temporary allowed
folder and drives the real page in headless Chrome.

| STP case | Test | What it checks | Result |
|---|---|---|---|
| TC-UI-01 | `test_tc_ui_01_compress_a_folder` | Browse to a folder, the destination is suggested, the archive is written | pass |
| TC-UI-02 | `test_tc_ui_02_extract_restores_identical_files` | Extract through the UI; SHA-256 of every restored file equals the original | pass |
| TC-UI-03 | `test_tc_ui_03_progress_then_full_results` | Progress screen (status, %, elapsed), then size, ratio, time, speed, stored blocks and method table | pass |
| TC-UI-04 | 6 tests | Not an archive, corrupted archive, path outside the allowed folder, unwritable destination, empty form, Try again keeps input. Plain-language text only, no `E_*` code | pass |
| TC-UI-05 | `test_tc_ui_05_page_stays_responsive_during_a_long_job` | 80 MB job: the page answers script calls in under 1 s throughout, progress advances and never goes backwards | pass |
| TC-UI-05 | `test_tc_ui_05_leaving_progress_keeps_the_job_running` | Leaving the progress screen keeps the page usable and the job still finishes | pass |
| TC-INFO-01 | `test_tc_info_01_info_through_the_ui` | Info shows the file count and every file of a folder archive; nothing extracted | pass |
| TC-VER-01 | `test_tc_ver_01_verify_through_the_ui` | Verify reports "Archive is OK"; nothing written | pass |
| NFR-04 | `test_keyboard_only_compress_flow`, `test_every_input_has_a_label` | Keyboard-only flow, focus moves to each screen heading, every input labelled | pass |
| - | `test_demo_mode_*` (2) | Demo mode works with no backend and labels results; `?fail` shows the error path | pass |

## 5. Defects found by these tests

| # | Found by | Defect | Status |
|---|---|---|---|
| D-01 | TC-UI-03 / TC-UI-05 | Typing a source and then clicking the destination box made the page insert a suggested destination at that moment, so the typed destination was appended to it (`data.csv.uatc/home/...`) and the job failed with "We could not find that file or folder." | Fixed: the destination is suggested only when the source is picked with Browse |
| D-02 | coverage run | gcov reported negative branch counts in multi-threaded code (counter race), breaking the report | Fixed in the coverage build with `-fprofile-update=atomic` |
| D-03 | earlier, CMake run | The Makefile builds tests with `-DNDEBUG`, so `assert()` is disabled in `make test`; with asserts on, 7 core suites fail | Open, reported to the core owner |

A note for test data: a file whose first four bytes are the text `UATC`
matches the archive magic number, so Info/Verify treat it as an archive
from an unknown version and report E_UNSUPPORTED_VERSION. This is correct
behaviour; the UI test data avoids that prefix.

Defects D-04 to D-06, found later by the system validation run, are in
`docs/validation-report.md` section 5.
