#!/usr/bin/env bash
# UATC system validation (STP Appendix A, system / performance / security
# levels). Runs the release CLI against a generated corpus and prints one
# PASS/FAIL line per check, with the evidence, so the output can be pasted
# straight into docs/validation-report.md.
#
#   scripts/system_validation.sh            uses build/uatc_cli (make cli)
#   UATC_CLI=/path/to/uatc scripts/system_validation.sh
#
# Covers: TC-NFR-01 (full corpus round trip, every STP data category),
# TC-ARC-03/04 (folders, edge sizes), TC-PAR-01/02 (1 vs 8 workers,
# repeated runs), TC-PAR-04 (throughput by worker count), TC-NFR-02
# (comparison with gzip -6 and, if the Python zstandard module is present,
# zstd -3), TC-INT-01/02, TC-ERR-01/02/03 (damaged archives and exit codes),
# TC-NFR-07 (statistics reported), TC-VER-01, TC-INFO-01.
#
# Exit status: 0 when every check passes.
set -uo pipefail

cd "$(dirname "$0")/.."
CLI="${UATC_CLI:-build/uatc_cli}"
[[ -x "$CLI" ]] || make cli >/dev/null || { echo "cannot build $CLI" >&2; exit 2; }
CLI="$(cd "$(dirname "$CLI")" && pwd)/$(basename "$CLI")"

WORK="$(mktemp -d -t uatc-validation-XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
FAILED=0
pass() { printf 'PASS  %-10s %s\n' "$1" "$2"; }
fail() { printf 'FAIL  %-10s %s\n' "$1" "$2"; FAILED=1; }

echo "UATC system validation - $(date -u '+%Y-%m-%d %H:%M UTC')"
echo "machine: $(uname -sm), $(nproc) CPUs; cli: $CLI"
echo

# ------------------------------------------------------------- corpus
# One file per STP section 6 data category, plus the size edge cases.
python3 - "$WORK/corpus" <<'PY'
import gzip, math, os, struct, sys, random
out = sys.argv[1]
rng = random.Random(2026)
def w(rel, data):
    p = os.path.join(out, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "wb").write(data)
words = ["the","of","and","to","in","is","archive","block","data","compress",
         "file","value","system","transform","stored","entropy","checksum"]
text = " ".join(rng.choice(words) for _ in range(400_000)).encode()
w("text/book.txt", text)
w("text/server.log", "".join(
    f"2026-11-{i%28+1:02d} 10:{i%60:02d}:{i%60:02d} INFO request id={i} status=200 ms={i%97}\n"
    for i in range(60_000)).encode())
w("text/source.cpp", (b"int f(int x) {\n    return x * 2 + 1;\n}\n" * 20_000))
w("numeric/timeseries.csv", "".join(
    f"{1700000000+i},{20+5*math.sin(i/300):.3f},{i%1000}\n" for i in range(120_000)).encode())
w("numeric/int32_ramp.bin", b"".join(struct.pack("<i", i * 3) for i in range(300_000)))
w("numeric/float64.bin", b"".join(struct.pack("<d", math.cos(i/40)) for i in range(150_000)))
w("numeric/sorted_ids.bin", b"".join(struct.pack("<Q", 10_000_000 + i * 7) for i in range(100_000)))
rec = b"".join(struct.pack("<IHH16s", i, i % 7, 0xBEEF, b"name-%08d" % i) for i in range(50_000))
w("structured/records.db", rec)
w("compressed/already.gz", gzip.compress(text, mtime=0))
w("compressed/random.bin", rng.randbytes(1_000_000))
w("synthetic/zeros.bin", b"\0" * 2_000_000)
w("synthetic/pattern.bin", b"ABCD1234" * 250_000)
w("synthetic/mixed.bin", b"\0" * 300_000 + rng.randbytes(300_000) + text[:300_000])
B = 1 << 20
w("edge/empty.bin", b"")
w("edge/one_byte.bin", b"x")
w("edge/block_minus_1.bin", rng.randbytes(B - 1))
w("edge/block_exact.bin", rng.randbytes(B))
w("edge/block_plus_1.bin", rng.randbytes(B + 1))
for i in range(200):
    w(f"tree/many/tiny_{i:03d}.txt", f"tiny file {i}\n".encode())
w("tree/deep/a/b/c/d/leaf.txt", b"deep\n")
w("tree/names/file with spaces.txt", b"spaces\n")
w("tree/names/naïve résumé ünïcödé.txt", "unicode\n".encode())
PY
FILES=$(cd "$WORK/corpus" && find . -type f ! -path './tree/*' | sort)
echo "corpus: $(echo "$FILES" | wc -l) files + 1 folder tree ($(find "$WORK/corpus/tree" -type f | wc -l) files)"
echo

# ------------------------------------------- TC-NFR-01 round trip per file
echo "## Round trip, every file (TC-NFR-01, TC-ARC-04, TC-NFR-07)"
printf '%-30s %12s %12s %7s %s\n' file original archive ratio result
for rel in $FILES; do
    src="$WORK/corpus/$rel"; arc="$WORK/a/$rel.uatc"; out="$WORK/x/$rel.d"
    mkdir -p "$(dirname "$arc")" "$out"
    if ! "$CLI" compress "$src" "$arc" --quiet 2>"$WORK/err"; then
        fail NFR-01 "$rel: compress failed: $(cat "$WORK/err")"; continue
    fi
    "$CLI" verify "$arc" >/dev/null 2>&1 || { fail VER-01 "$rel: verify failed"; continue; }
    "$CLI" extract "$arc" "$out" --quiet >/dev/null 2>&1 || { fail NFR-01 "$rel: extract failed"; continue; }
    restored="$out/$(basename "$rel")"
    o=$(stat -c%s "$src"); a=$(stat -c%s "$arc")
    ratio=$(awk -v o="$o" -v a="$a" 'BEGIN{printf "%.2f", a ? o/a : 0}')
    if [[ "$(sha256sum <"$src")" = "$(sha256sum <"$restored")" ]]; then r=identical; else r=DIFFERENT; FAILED=1; fi
    printf '%-30s %12s %12s %7s %s\n' "$rel" "$o" "$a" "$ratio" "$r"
done
[[ "$FAILED" -eq 0 ]] && pass NFR-01 "every file verified and restored with identical SHA-256"
echo

# ------------------------------------------------- TC-ARC-03 folder tree
echo "## Folder tree (TC-ARC-03, TC-FS-02, TC-INFO-01)"
"$CLI" compress "$WORK/corpus/tree" "$WORK/tree.uatc" --quiet
files_in=$(find "$WORK/corpus/tree" -type f | wc -l)
info_files=$("$CLI" info "$WORK/tree.uatc" | awk -F': +' '/^Files/{print $2}')
"$CLI" extract "$WORK/tree.uatc" "$WORK/tree_out" --quiet >/dev/null
if diff -r "$WORK/corpus/tree" "$WORK/tree_out" >/dev/null && [[ "$info_files" = "$files_in" ]]; then
    pass ARC-03 "$files_in files (nested, spaces, Unicode names, 200 tiny files) restored identically; info reports $info_files"
else
    fail ARC-03 "tree differs or info reports $info_files of $files_in files"
fi
echo

# ----------------------------------------------- TC-PAR-01/02 determinism
echo "## Determinism (TC-PAR-01, TC-PAR-02)"
big="$WORK/big.csv"
python3 -c "
import sys
with open(sys.argv[1],'w') as f:
    for i in range(1_000_000): f.write(f'{i},sensor-{i%97},{(i*37)%1000}.{i%10},ok\n')" "$big"
"$CLI" compress "$big" "$WORK/w1.uatc" --workers 1 --quiet
ref=$(sha256sum <"$WORK/w1.uatc")
same=1
for run in $(seq 1 5); do
    "$CLI" compress "$big" "$WORK/w8.uatc" --workers 8 --quiet
    [[ "$(sha256sum <"$WORK/w8.uatc")" = "$ref" ]] || same=0
done
if [[ "$same" -eq 1 ]]; then
    pass PAR-01 "workers=1 and 5 runs of workers=8 give byte-identical archives ($(stat -c%s "$big") B input)"
else
    fail PAR-01 "archives differ between worker counts"
fi
echo

# --------------------------------------------------- TC-PAR-04 throughput
echo "## Throughput by worker count (TC-PAR-04, recorded not pass/failed)"
size_mb=$(awk -v b="$(stat -c%s "$big")" 'BEGIN{printf "%.1f", b/1048576}')
printf '%-8s %10s %10s\n' workers seconds MB/s
for wk in 1 2 4 8; do
    t0=$(date +%s.%N)
    "$CLI" compress "$big" "$WORK/t.uatc" --workers "$wk" --quiet
    t1=$(date +%s.%N)
    awk -v w="$wk" -v a="$t0" -v b="$t1" -v m="$size_mb" \
        'BEGIN{d=b-a; printf "%-8s %10.2f %10.1f\n", w, d, m/d}'
done
echo "(input $size_mb MB, $(nproc) CPUs)"
echo

# ------------------------------------------------ TC-NFR-02 vs gzip / zstd
echo "## Ratio against gzip -6 and zstd -3 (TC-NFR-02, recorded not pass/failed)"
printf '%-30s %8s %8s %8s\n' file uatc gzip-6 zstd-3
python3 -c "import zstandard" 2>/dev/null && HAVE_ZSTD=1 || HAVE_ZSTD=0
for rel in text/book.txt text/server.log numeric/timeseries.csv numeric/int32_ramp.bin \
           structured/records.db compressed/random.bin synthetic/zeros.bin; do
    src="$WORK/corpus/$rel"; o=$(stat -c%s "$src")
    u=$(stat -c%s "$WORK/a/$rel.uatc")
    g=$(gzip -6 -c "$src" | wc -c)
    if [[ "$HAVE_ZSTD" -eq 1 ]]; then
        z=$(python3 -c "import sys,zstandard;print(len(zstandard.ZstdCompressor(level=3).compress(open(sys.argv[1],'rb').read())))" "$src")
        zr=$(awk -v o="$o" -v a="$z" 'BEGIN{printf "%.2f", o/a}')
    else zr="n/a"; fi
    awk -v f="$rel" -v o="$o" -v u="$u" -v g="$g" -v z="$zr" \
        'BEGIN{printf "%-30s %8.2f %8.2f %8s\n", f, o/u, o/g, z}'
done
echo "(ratio = original / compressed; higher is smaller output)"
echo

# ------------------------------------ TC-INT / TC-ERR damaged archives
echo "## Damaged archives (TC-INT-01, TC-INT-02, TC-ERR-01/02/03)"
arc="$WORK/a/text/book.txt.uatc"
expect_exit() {   # id, expected exit codes (|-separated), file, description
    "$CLI" verify "$3" >/dev/null 2>"$WORK/err"; rc=$?
    msg=$(head -c 120 "$WORK/err")
    if [[ "|$2|" == *"|$rc|"* ]]; then pass "$1" "$4 -> exit $rc ($msg)"
    else fail "$1" "$4 -> exit $rc, expected $2 ($msg)"; fi
}
cp "$arc" "$WORK/flip.uatc"
python3 - "$WORK/flip.uatc" <<'PY'
import sys; p=sys.argv[1]; b=bytearray(open(p,'rb').read()); b[len(b)//2]^=0x5A; open(p,'wb').write(b)
PY
expect_exit INT-01 "13|14" "$WORK/flip.uatc" "one payload byte flipped"
cp "$arc" "$WORK/footer.uatc"
python3 - "$WORK/footer.uatc" <<'PY'
import sys; p=sys.argv[1]; b=bytearray(open(p,'rb').read()); b[-20]^=0xFF; open(p,'wb').write(b)
PY
expect_exit INT-02 "14|10" "$WORK/footer.uatc" "archive checksum region changed"
for cut in 10 60 200 1000; do
    head -c "$cut" "$arc" >"$WORK/trunc.uatc"
    expect_exit ERR-01 "10|12" "$WORK/trunc.uatc" "truncated to $cut bytes"
done
cp "$arc" "$WORK/ver.uatc"
printf '\x63\x00' | dd of="$WORK/ver.uatc" bs=1 seek=4 conv=notrunc status=none
expect_exit ERR-02 "11" "$WORK/ver.uatc" "format version set to 99"
expect_exit ERR-03 "10" "$WORK/corpus/text/book.txt" "a text file, not an archive"
"$CLI" extract "$WORK/flip.uatc" "$WORK/flip_out" >/dev/null 2>&1; rc=$?
if [[ "$rc" -eq 13 ]] || [[ "$rc" -eq 14 ]]; then
    pass INT-01 "extract of the flipped archive also exits $rc, nothing restored"
else
    fail INT-01 "extract of the flipped archive exits $rc (expected 13/14)"
fi
echo

# --------------------------------------------- TC-SEC-01 path traversal
# The recorded path is replaced with "../evil.bin" and the archive SHA-256
# is recomputed, so every checksum is valid and only the path rule can stop
# the write outside the destination.
echo "## Path traversal with a valid checksum (TC-SEC-01)"
mkdir -p "$WORK/sec"
printf 'A%.0s' $(seq 1 2000) >"$WORK/sec/victim1.bin"
"$CLI" compress "$WORK/sec/victim1.bin" "$WORK/sec/evil.uatc" --quiet
python3 - "$WORK/sec/evil.uatc" <<'PY'
import hashlib, struct, sys
p = sys.argv[1]; b = bytearray(open(p, "rb").read())
ft = struct.unpack_from("<Q", b, 16)[0]
at = struct.unpack_from("<Q", b, ft + 4)[0]; n = struct.unpack_from("<I", b, ft + 12)[0]
assert n == len(b"../evil.bin")
b[at:at + n] = b"../evil.bin"
body = len(b) - 76                      # 2 x SHA-256 + 12-byte footer
b[body + 32:body + 64] = hashlib.sha256(bytes(b[:body])).digest()
open(p, "wb").write(b)
PY
expect_exit SEC-01 "15" "$WORK/sec/evil.uatc" "verify, ../ path with a recomputed checksum"
mkdir -p "$WORK/sec/dest"
"$CLI" extract "$WORK/sec/evil.uatc" "$WORK/sec/dest" >/dev/null 2>&1; rc=$?
if [[ "$rc" -eq 15 ]] && [[ ! -e "$WORK/sec/evil.bin" ]] && [[ -z "$(ls -A "$WORK/sec/dest")" ]]; then
    pass SEC-01 "extract refused with exit 15; nothing written inside or outside the destination"
else
    fail SEC-01 "extract exit $rc; evil.bin outside: $([[ -e "$WORK/sec/evil.bin" ]] && echo YES || echo no)"
fi
echo

# --------------------------------------------- TC-SEC-02 absurd sizes
# Header fields claiming billions of blocks or files must be rejected
# before any allocation. Run under a 256 MB memory cap: an allocation of
# what the header claims would fail the check by crashing, not by message.
echo "## Absurd declared sizes, under a 256 MB memory limit (TC-SEC-02)"
for field in "12:block_count" "8:file_count"; do
    off="${field%%:*}"; name="${field##*:}"
    cp "$arc" "$WORK/huge.uatc"
    printf '\xff\xff\xff\xff' | dd of="$WORK/huge.uatc" bs=1 seek="$off" conv=notrunc status=none
    t0=$(date +%s.%N)
    ( ulimit -v 262144; "$CLI" verify "$WORK/huge.uatc" ) >/dev/null 2>"$WORK/err"; rc=$?
    secs=$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.2f", b-a}')
    if [[ "$rc" -eq 10 ]]; then
        pass SEC-02 "$name = 4294967295 -> exit 10 in ${secs}s within 256 MB ($(head -c 80 "$WORK/err"))"
    else
        fail SEC-02 "$name = 4294967295 -> exit $rc ($(head -c 80 "$WORK/err"))"
    fi
done
echo

if [[ "$FAILED" -eq 0 ]]; then echo "RESULT: all checks passed"; else echo "RESULT: FAILURES above"; fi
exit "$FAILED"
