# UATC - Product Maintenance Plan

**Owner**: Manasa K (QA Lead), with the module owners below.
**Applies to**: the UATC core library, CLI, HTTP API, web frontend, archive
format and the test/CI tooling.
**Related**: `docs/validation-report.md` (current state and open defects),
`docs/rtm.md` (what each change must keep passing).

## 1. Goals

1. Never lose data: every release must keep byte-for-byte round trips and
   must still read every archive version it claims to support.
2. Keep each change small, reviewed and traceable to a requirement or a
   defect.
3. Keep the quality gates green: build, tests, coverage, UI tests,
   SonarCloud.

## 2. Ownership

Changes go through the owner of the module (SAD 3.4), who reviews the PR.

| Area | Paths | Owner | Backup reviewer |
|---|---|---|---|
| Compression core (transforms, coders, probe, selector, facade) | `src/core/`, `include/core/` | Sai Rishi Gangarapu | Sampada K |
| Archive and file-system layer, archive format | `src/archive/`, `src/filesystem/`, `include/archive/` | S Chakshu | Sai Rishi Gangarapu |
| Thread pool, benchmark, performance | `src/core/thread_pool.cpp`, `pcomp/`, benchmark command | Sampada K | Sai Rishi Gangarapu |
| Inspector/verifier, HTTP API, web UI, CLI integration | `src/archive/archive_{inspector,verifier}.cpp`, `src/server/`, `web/`, `src/cli/` | Manasa K | S Chakshu |
| Tests, CI, quality reports | `tests/`, `scripts/`, `Jenkinsfile`, `docs/*report*.md` | Manasa K | all |

## 3. Kinds of maintenance

| Kind | Examples for UATC | How it is handled |
|---|---|---|
| Corrective (bugs) | D-03 test assertions, D-04 empty folders | Issue, then a fix PR with a regression test that fails before the fix |
| Adaptive (environment) | New compiler, new Ubuntu image, new browser for Selenium, Windows support | Update the Dockerfile and Jenkins image first; run the full suite on the new environment |
| Perfective (features, speed) | A new transform (bit-plane, ID 5), LZ matching for text, rANS coder, benchmark screen in the UI | Issue with a user story; behind the existing interfaces; benchmark before and after |
| Preventive | Fuzzing the reader, raising branch coverage, removing duplicated code, dependency updates | Scheduled in each sprint's last day |

## 4. Change workflow

1. **Issue** on GitHub: user story or defect, requirement ID (FR/NFR),
   severity, owner.
2. **Branch** from `main`: `<name>/<short-topic>`. Never commit to `main`.
3. **Change + test**: every behaviour change comes with a test; every defect
   fix with a test that failed before the fix.
4. **Local checks**: `make all && make test`; `make ui-test` for web or API
   changes; `scripts/system_validation.sh` for core, archive or CLI changes.
5. **Pull request**: describe what and why, link the issue, name the
   requirement, request the module owner's review.
6. **CI gates** (all must be green before merge):

   | Gate | Where | Blocks merge when |
   |---|---|---|
   | Build + unit/integration tests | Jenkins `Build`, `Test` | any suite fails |
   | Coverage | Jenkins `Coverage` | core < 80% lines or < 70% branches |
   | UI tests | Jenkins `UI tests (Selenium)` | any UI test fails or no browser |
   | Static analysis | SonarCloud quality gate | rating below A on new code |
   | Review | GitHub | no approval from the module owner |

7. **Merge** by the reviewer, not the author.

## 5. Defect handling

| Severity | Meaning | Target |
|---|---|---|
| Critical | Wrong output or data loss (round trip differs, corruption undetected), security hole (path escape, API exposure) | Fix before any other work; no release while open |
| High | A requirement fails, or a quality gate is ineffective (for example D-03) | Fix within the current sprint |
| Medium | Feature works with a limitation (for example D-04 empty folders) | Plan into the next sprint |
| Low | Cosmetic, wording, minor inconsistency | Backlog |

Every defect report contains: steps to reproduce, expected and actual
result, the test case it relates to, and the archive or input that shows it
(attach it if small). Defects found by tests are recorded in
`docs/validation-report.md` section 5.

## 6. Archive format compatibility (the most important rule)

Old archives are user data and must stay readable.

- Transform IDs and codec IDs are **never reused or renumbered** (SAD 4.1).
  A new transform takes the next free ID (5 is reserved for bit-plane).
- Adding a transform or codec does **not** change the format version: older
  readers reject the unknown ID with a clear error.
- A layout change (new header field, new region) increments the format
  **major** version. The reader keeps reading every older major version it
  ever supported; an archive from a newer major version is rejected with
  E_UNSUPPORTED_VERSION.
- Keep a small set of archives produced by each released version under
  `tests/` as fixtures; a format change must still read all of them.
- Any change to the reader or writer runs the full validation script,
  including the damaged-archive and path-traversal checks.

## 7. Regression testing

| When | What runs |
|---|---|
| Every push (Jenkins) | build, all C++ suites, coverage gate, Selenium UI suite, SonarCloud |
| Before merging a core/archive change | plus `scripts/system_validation.sh` |
| Before a release / demo | everything above, plus a clean clone build (`git clone`, `make all`, `make test`), plus the 15-minute demo dry run (`docs/demo-script.md`) |
| Weekly | ThreadSanitizer build of the thread pool, parallel and API tests |

## 8. Security maintenance

- The HTTP API stays loopback-only, token-protected, Origin- and
  Host-checked, and limited to the allowed folder (SAD 3.9). Any new
  endpoint must go through the same path rule (`resolve_in_root`) and the
  same JSON parser, and gets an entry in `tests/server/api_test.cpp`.
- Any new code that writes files from an archive goes through
  `safe_relative_path` and is covered by a TC-SEC-01 style test with a
  recomputed checksum.
- SonarCloud security findings are fixed, not accepted, unless the owner
  documents why in the PR.
- Planned: a mutation fuzzer for the archive reader (TC-NFR-03).

## 9. Toolchain and dependency updates

UATC has no third-party runtime libraries. Tooling:

| Tool | Pinned in | Update rule |
|---|---|---|
| Ubuntu image, GCC, CMake | `Dockerfile`, `Jenkinsfile` | Update once per semester phase; full suite must pass |
| Python test tools (selenium, pytest, gcovr) | `tests/ui/requirements.txt` | Bounded version ranges; update deliberately |
| Chrome / ChromeDriver | Selenium Manager in CI | Fetched to match automatically |

## 10. Documentation kept in step with the code

| When this changes | Update |
|---|---|
| A requirement, test case or code location | `docs/rtm.md` |
| API endpoint, request or error shape | `docs/api-contract.md` |
| UI screen or element ID | `web/README.md`, `docs/wireframes.md`, `tests/ui/` |
| CLI command or option | `docs/cli-spec.md`, `uatc --help` |
| Archive layout | SAD 4.1 and `include/archive/archive_format.hpp` together |
| Test results | `docs/test-report.md`, `docs/validation-report.md` |

## 11. Current maintenance backlog

| Item | Kind | Owner | From |
|---|---|---|---|
| Enable assertions in `make test` and fix the 7 failing core assertions | Corrective (High) | Sai Rishi | D-03 |
| Store empty directories in folder archives | Corrective (Medium) | S Chakshu | D-04 |
| Archive reader fuzzer under ASan/UBSan | Preventive | Manasa | TC-NFR-03 |
| Dummy-transform test (add a transform without touching other layers) | Preventive | Sai Rishi | TC-NFR-05 |
| Docker and Windows test runs | Adaptive | Sampada | TC-NFR-06 |
| Benchmark results screen in the web UI (needs an API endpoint) | Perfective | Manasa | Execution plan, Day 17 |
| LZ-style matching to close the text-compression gap with gzip/zstd | Perfective | Sai Rishi | Validation report 4.2 |
| Bit-plane transform (ID 5) for floating-point data | Perfective | Sai Rishi | Validation report 4.2 |
