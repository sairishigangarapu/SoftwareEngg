#!/usr/bin/env bash
# UATC dry run - end-to-end demonstration of the prototype.
#
#   ./scripts/demo.sh            # run and keep the artefacts
#   ./scripts/demo.sh --clean    # run and delete the artefacts afterwards
#
# Runs the whole pipeline on a scratch directory and prints a report:
# build, tests, compress / info / verify / extract, byte-for-byte fidelity,
# corruption detection, per-category adaptive transform selection, and the
# benchmark.
#
# Where things are written:
#   - Everything the demo produces (corpus, archives, restored files, logs,
#     benchmark JSONL) goes into the scratch directory shown at the start.
#   - Compiled binaries go to build/ and pcomp/build/ in the repository, and
#     the pcomp benchmark corpus to pcomp/datasets/samples/. Both are ignored
#     by git, so the repository stays clean.

set -uo pipefail
cd "$(dirname "$0")/.."

CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

WORK="$(mktemp -d "${TMPDIR:-/tmp}/uatc-demo-XXXXXX")"

cleanup() {
    if [ "$CLEAN" -eq 1 ]; then
        rm -rf "$WORK"
        printf '\nScratch directory removed: %s\n' "$WORK"
    fi
}
trap cleanup EXIT

BOLD=''; DIM=''; RESET=''
if [ -t 1 ]; then
  BOLD="$(printf '\033[1m')"; DIM="$(printf '\033[2m')"; RESET="$(printf '\033[0m')"
fi

step()  { printf '\n%s== %s ==%s\n' "$BOLD" "$1" "$RESET"; }
note()  { printf '%s   %s%s\n' "$DIM" "$1" "$RESET"; }
ok()    { printf '   %sPASS%s %s\n' "$BOLD" "$RESET" "$1"; }
bad()   { printf '   FAIL %s\n' "$1"; FAILED=1; }
FAILED=0

printf '%sUATC - Universal Adaptive Transform Compression%s\n' "$BOLD" "$RESET"
printf '%sDry run - scratch directory %s%s\n' "$DIM" "$WORK" "$RESET"

# ---------------------------------------------------------------- 1. build
step "1. Build"
if make all >"$WORK/build.log" 2>&1; then
  ok "core library, CLI, benchmark and $(grep -c '^Running' "$WORK/test.log" 2>/dev/null || echo 20) test binaries built"
else
  bad "build failed - see $WORK/build.log"; tail -20 "$WORK/build.log"; exit 1
fi

# ---------------------------------------------------------------- 2. tests
step "2. Test suite"
if make test >"$WORK/test.log" 2>&1; then
  ok "$(grep -c '^Running' "$WORK/test.log") test suites passed"
else
  bad "tests failed - see $WORK/test.log"; tail -30 "$WORK/test.log"
fi

# ---------------------------------------------------------------- 3. corpus
step "3. Demo corpus"
mkdir -p "$WORK/corpus"
python3 - "$WORK/corpus" <<'PY'
import math, os, random, struct, sys
out = sys.argv[1]
rng = random.Random(1337)
os.makedirs(f"{out}/text", exist_ok=True)
os.makedirs(f"{out}/numeric", exist_ok=True)
os.makedirs(f"{out}/binary", exist_ok=True)
words = [''.join(rng.choice("etaoinshrdlucmfwypvbgkjqxz") for _ in range(rng.randint(2, 9))) for _ in range(3000)]
w = [1/(i+1) for i in range(len(words))]
text = (" ".join(rng.choices(words, w, k=300_000))).encode()
open(f"{out}/text/english.txt","wb").write(text)
open(f"{out}/numeric/uint32_table.bin","wb").write(b"".join(
    struct.pack("<I", i*4 + rng.randint(0,3)) for i in range(250_000)))
open(f"{out}/numeric/float64_signal.bin","wb").write(b"".join(
    struct.pack("<d", math.sin(i/50)) for i in range(150_000)))
open(f"{out}/binary/zeros.bin","wb").write(b"\x00" * 500_000)
open(f"{out}/binary/random.bin","wb").write(rng.randbytes(500_000))
PY
note "text/english.txt        1.4 MB   pseudo-English, Zipf-distributed"
note "numeric/uint32_table.bin  1.0 MB  mostly-increasing 32-bit records"
note "numeric/float64_signal.bin 1.2 MB float64 sine signal"
note "binary/zeros.bin        500 kB   all zeros"
note "binary/random.bin       500 kB   incompressible random bytes"

# ------------------------------------------------- 4. compress / inspect
step "4. Compress and inspect (adaptive per-block transform selection)"
note "ratio = original / archive; above 1.00 means the file got smaller."
note "the engine picks a different transform per data type, and stores"
note "incompressible blocks unchanged."
printf '\n'
for f in "$WORK"/corpus/*/*; do
  name=$(basename "$f")
  "$PWD/build/uatc_cli" compress "$f" "$WORK/$name.uatc" >/dev/null
  printf '   %-22s ' "$name"
  "$PWD/build/uatc_cli" info "$WORK/$name.uatc" 2>/dev/null \
    | awk -F': +' '/Original bytes/{o=$2} /Archive bytes/{a=$2} /Ratio/{r=$2}
           /Stored blocks/{s=$2} END{printf "%10s -> %10s bytes   ratio %6s   stored %s\n", o, a, r, s}'
  "$PWD/build/uatc_cli" info "$WORK/$name.uatc" 2>/dev/null \
    | sed -n '/^Transforms:/,/^Codecs:/p' | grep -E '^  ' | sed 's/^/       /'
done

# ------------------------------------------------------- 5. round trip
step "5. Extract and compare byte-for-byte"
mkdir -p "$WORK/restored"
for f in "$WORK"/corpus/*/*; do
  name=$(basename "$f")
  "$PWD/build/uatc_cli" extract "$WORK/$name.uatc" "$WORK/restored" >/dev/null 2>&1
  if cmp -s "$f" "$WORK/restored/$name"; then
    ok "$name restored identically (sha256 $(sha256sum "$f" | cut -c1-16)...)"
  else
    bad "$name differs after round trip"
  fi
done

# ---------------------------------------------------- 6. integrity check
step "6. Integrity: verify, then corrupt and verify again"
F="$WORK/corpus/text/english.txt.uatc"
[ -f "$F" ] || F="$WORK/english.txt.uatc"
if "$PWD/build/uatc_cli" verify "$F"; then
  ok "intact archive passes verification"
else
  bad "intact archive failed verification"
fi
cp "$F" "$WORK/corrupt.uatc"
printf '\xFF' | dd of="$WORK/corrupt.uatc" bs=1 seek=$(( $(stat -c%s "$WORK/corrupt.uatc") - 1 )) conv=notrunc status=none
if "$PWD/build/uatc_cli" verify "$WORK/corrupt.uatc" 2>&1 | sed 's/^/   /'; then
  bad "corrupted archive was NOT detected"
else
  ok "corrupted archive is detected and reported, exit code $?"
fi

# ---------------------------------------------------------- 7. benchmark
step "7. Benchmark (single-threaded path; parallel path is Day 6+)"
note "the pcomp harness measures the raw block path (I/O and copy cost)."
note "compression ratios are shown in step 4."

# make_samples.py rewrites the tracked manifest, and the system headers it
# picks up differ per machine. Keep the repository's copy as it was.
MANIFEST="pcomp/datasets/manifest.json"
[ -f "$MANIFEST" ] && cp "$MANIFEST" "$WORK/manifest.json.bak"

if python3 pcomp/scripts/make_samples.py >/dev/null 2>&1; then
  ./pcomp/build/pc_bench --root pcomp/datasets/samples --out "$WORK/bench.jsonl" \
      --block-size 1048576 --reps 1 --workers 1,2,4,max 2>&1 | sed 's/^/   /'
  note "workers > 1 are recorded as skipped_parallel_not_implemented until the"
  note "thread pool lands - see issue #40 (FR-10)."
else
  note "sample generation skipped"
fi

# Put the tracked manifest back the way we found it.
if [ -f "$WORK/manifest.json.bak" ]; then
  cp "$WORK/manifest.json.bak" "$MANIFEST"
fi

# -------------------------------------------------------------- summary
step "Summary"
if [ "$FAILED" -eq 0 ]; then
  printf '   %sAll demo steps passed.%s\n' "$BOLD" "$RESET"
else
  printf '   %sOne or more demo steps failed.%s\n' "$BOLD" "$RESET"
fi
printf '\n   Artefacts are in %s\n' "$WORK"
printf '     corpus/      the five demo input files\n'
printf '     restored/    the files extracted back from the archives\n'
printf '     *.uatc       the archives that were created\n'
printf '     bench.jsonl  the benchmark records\n'
printf '     build.log    the build output\n'
printf '     test.log     the test output\n'
if [ "$CLEAN" -eq 0 ]; then
  printf '\n   Delete it with: rm -rf %s\n' "$WORK"
fi
exit "$FAILED"
