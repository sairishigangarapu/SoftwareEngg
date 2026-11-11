# UATC - Universal Adaptive Transform Compression

A block-adaptive, lossless file compression and archival tool in C++17,
with a command-line interface and a browser-based frontend.

Level 3 Software Engineering Mini Project (UE24CS341A), PES University.
Guide: Dr. Sapna V M.

## What it does

- Compresses a **file or a whole folder** into one `.uatc` archive and
  extracts it back **byte for byte**.
- Splits input into blocks and, **for each block**, picks the reversible
  transform (None, Delta, XOR, Byte-shuffle, RLE) with the lowest estimated
  cost before entropy coding (Huffman or a baseline coder).
- **Stores** a block unchanged when compressing it would not make it
  smaller, so already-compressed or random data is never inflated beyond
  the archive's fixed metadata.
- Processes independent blocks on a **thread pool**; the archive is
  identical for any worker count.
- Checks **integrity** with CRC32 per block and SHA-256 per file and per
  archive; every kind of damage is reported with its own error code.
- Offers `info` and `verify` without extracting, and a local **web UI**
  for non-technical users.

UATC does not try to beat gzip or Zstandard; it is measured honestly
against them (see `docs/validation-report.md`).

## Quick start

Requirements: a C++17 compiler (GCC or Clang), `make` (or CMake 3.16+),
Python 3 for the scripts and UI tests.

```bash
make all
```
```bash
make test
```

The CLI is built at `build/uatc_cli`:

```bash
build/uatc_cli compress ./project ./project.uatc --workers 4
```
```bash
build/uatc_cli info ./project.uatc
```
```bash
build/uatc_cli verify ./project.uatc
```
```bash
build/uatc_cli extract ./project.uatc ./restored
```

All commands and options: `build/uatc_cli --help` and `docs/cli-spec.md`.

### Web UI

```bash
build/uatc_cli serve --root ~/Documents
```

Open the address it prints (`http://127.0.0.1:8080/?live#token=...`). The
server listens on 127.0.0.1 only, needs the printed session token, and can
only see the folder given with `--root`. Without a backend, open
`web/index.html` through any static server for a demo mode. Details:
`web/README.md`.

## Testing and quality

| What | Command | Result (latest run) |
|---|---|---|
| C++ unit and integration tests | `make test` | 32 suites pass |
| Selenium UI tests | `pip install -r tests/ui/requirements.txt && make ui-test` | 17 pass |
| Line and branch coverage | `make coverage` | core 91.0% lines, 81.1% branches |
| System validation (corpus, damage, security, performance) | `scripts/system_validation.sh` | all checks pass |
| Archive-reader fuzzing under ASan/UBSan | `make fuzz` | 45,250 damaged archives, all handled |
| Static analysis | SonarCloud quality gate on every PR | |
| CI | `Jenkinsfile`: build, test, fuzz, coverage, UI tests, SonarQube, benchmark, demo | |

Reports: `docs/test-report.md` (unit results and coverage),
`docs/validation-report.md` (every STP test case, performance, defects),
`docs/rtm.md` (requirements traceability).

## Repository layout

| Path | Contents |
|---|---|
| `include/`, `src/core/` | Compression core: block statistics, probe, transforms, selector, coders, thread pool, the compress/extract facade |
| `src/archive/`, `src/filesystem/` | Archive format writer/reader (single file and folder), inspector, verifier, file enumeration |
| `src/server/` | Local HTTP API and job manager used by the web UI |
| `src/cli/` | The `uatc` command-line tool |
| `web/` | Web frontend (HTML, CSS, vanilla JavaScript) |
| `tests/` | C++ tests (`tests/`, `tests/core/`, `tests/server/`) and Selenium UI tests (`tests/ui/`) |
| `scripts/` | Demo, coverage and system-validation scripts |
| `pcomp/` | Benchmark harness and samples |
| `docs/` | Specifications, contracts, reports, plans |
| `UATC_SRS.pdf`, `UATC_SAD.pdf`, `UATC_STP.pdf` | Requirements, architecture and design, test plan |

## Documents

| Stage | Document |
|---|---|
| Requirements | `UATC_SRS.pdf` |
| Architecture and design | `UATC_SAD.pdf`, `docs/core/pipeline.md`, `docs/api-contract.md`, `docs/wireframes.md` |
| Planning | `docs/sprints/SPRINT_PLAN.md`, GitHub issues |
| Testing | `UATC_STP.pdf`, `docs/test-report.md`, `docs/rtm.md` |
| Validation and maintenance | `docs/validation-report.md`, `docs/maintenance-plan.md` |
| Demo | `docs/demo-script.md`, `docs/DEMO.md`, `scripts/demo.sh` |

## Team

| Member | Area | Requirements |
|---|---|---|
| Sai Rishi Gangarapu (Lead) | Compression core and adaptive transform engine | FR-04 to FR-07 |
| S Chakshu | Archive and file-system layer | FR-01 to FR-03, FR-09 |
| Sampada K | Execution, performance and benchmarking | FR-10, performance |
| Manasa K | Inspection and verification, web frontend, API, QA | FR-11 to FR-15 |

## Contributing

Work on a branch, open a pull request into `main`, and get the module
owner's review; never push to `main` directly. The CI gates and the rules
for the archive format are in `docs/maintenance-plan.md`.
