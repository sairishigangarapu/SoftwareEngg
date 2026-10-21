# UATC Demo Runbook

**Purpose**: a repeatable dry run of the prototype to show the project guide.  
**Owner**: Sai Rishi Gangarapu (Team Lead)  
**Time needed**: about 3 minutes

## One command

```bash
./scripts/demo.sh
```

It builds the project, runs the whole test suite, generates a mixed test
corpus, compresses and inspects each file, extracts and compares byte for
byte, corrupts an archive to show integrity checking, and runs the benchmark.
Everything happens in a scratch directory that is deleted afterwards, so
nothing in the repository is touched.

> `cmake` is not installed on the lab machines, so the build goes through the
> root `Makefile`. If `cmake` is available, `cmake -S . -B build && cmake --build build`
> works as well.

## What each step shows, and what to say

| Step | What it demonstrates | Say |
|------|--------------------|-----|
| 1. Build | The C++17 core, archive layer, CLI and benchmark all compile from one command | "Everything is C++17 with CMake; this Makefile is the fallback for machines without cmake." |
| 2. Tests | 20 test suites, unit + integration, all green | "Every module has tests written alongside it, as the STP requires." |
| 3. Corpus | Five data shapes: text, structured numeric, float signal, zeros, random | "We test against several data categories, not just one." |
| 4. Compress + inspect | **The core idea**: a different transform is chosen per block, and incompressible data is stored unchanged | "This is the adaptive engine. Numeric records get byte-shuffle and shrink 3.4x. Text gets delta. Random bytes and gzip data are recognised as incompressible and stored as-is, so the archive never grows." |
| 5. Round trip | Every file restored **byte for byte**, SHA-256 shown | "Correctness is a hard requirement. Every extracted file matches the original exactly." |
| 6. Integrity | Flip one byte in a valid archive, verification catches it and reports the error code | "Corruption is always detected and reported, never silently turned into wrong output. The exit code maps to the error table in the SAD." |
| 7. Benchmark | Throughput on the raw path | "The benchmark harness is in place. The parallel path is the next milestone, so worker counts above one are recorded as skipped rather than faked." |

## Running the individual commands

If you prefer to drive it by hand instead of using the script:

```bash
make all                       # build everything
make test                      # run all 20 test suites

./build/uatc_cli --help

# Compress: the engine picks the transform per block
./build/uatc_cli compress pcomp/datasets/samples/numeric/uint32_table.bin demo.uatc

# Inspect: sizes, ratio and the transform breakdown
./build/uatc_cli info demo.uatc

# Verify: structure and every block checksum
./build/uatc_cli verify demo.uatc

# Extract: restores the original file
./build/uatc_cli extract demo.uatc ./restored
cmp pcomp/datasets/samples/numeric/uint32_table.bin ./restored/uint32_table.bin

# Error handling: a missing file gives the right code and exit status
./build/uatc_cli compress /nope/missing.bin out.uatc; echo "exit=$?"
```

## Exit codes (SAD 4.3.4)

```
0  success                     10 E_MALFORMED_ARCHIVE
2  E_INVALID_ARGUMENT         11 E_UNSUPPORTED_VERSION
3  E_NOT_FOUND                12 E_MISSING_BLOCK
4  E_PERMISSION               13 E_BLOCK_CHECKSUM
5  E_IO / E_DISK_FULL         14 E_ARCHIVE_CHECKSUM
                               15 E_PATH_TRAVERSAL
```

## The web frontend

The frontend is mock-mode by default, so it runs with no backend:

```bash
python3 -m http.server 8000 --directory web
# open http://localhost:8000
```

Add `?fail` to the URL to see the error screen. The API contract the frontend
targets is in `docs/api-contract.md`; the backend HTTP layer is not
implemented yet, so the frontend is still talking to mock data.

## What to say about the remaining work

Be straight about the gaps - the guide will check them anyway:

- **Parallel execution (FR-10) is not done.** The thread pool is declared but
  not implemented, so the benchmark records worker counts above one as
  `skipped_parallel_not_implemented`. Issue #40.
- **The entropy coder is still the baseline PackBits-style coder.** The
  interface is in place, so swapping in Huffman or rANS does not touch the
  compressor. This is why text compresses only slightly.
- **Checksums are FNV-1a, not the CRC32 and SHA-256 the SAD specifies.**
  Issue #41.
- **Multi-file and directory archives are not supported yet.** The reader
  currently handles one file per archive.
- **The HTTP API and job manager are not implemented.** The frontend runs on
  mock data.

## Artefacts worth keeping

- `pcomp/results/env.md` - the environment sheet (CPU, RAM, OS, compiler, build flags)
- `pcomp/results/bench_<stamp>.jsonl` - per-file timing records
- `docs/sprints/SPRINT_PLAN.md` and `docs/sprints/UATC-*.md` - the sprint and issue plan
- `docs/core/pipeline.md` - the per-block pipeline design
- GitHub issues #7-#34 (closed, the delivered work) and #40-#42 (open, the remaining work)
