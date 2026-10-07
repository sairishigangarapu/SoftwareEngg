#!/usr/bin/env bash
# Reproducible MVP benchmark: ./scripts/run_bench.sh [reps] [block_size]
set -euo pipefail
cd "$(dirname "$0")/.."
REPS="${1:-5}"; BLOCK="${2:-1048576}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j >/dev/null
[ -f datasets/manifest.json ] || python3 scripts/make_samples.py
./scripts/env_sheet.sh results/env.md
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
./build/pc_bench --root datasets/samples --out "results/bench_${STAMP}.jsonl" \
                 --block-size "$BLOCK" --reps "$REPS" --workers 1,2,4,max
