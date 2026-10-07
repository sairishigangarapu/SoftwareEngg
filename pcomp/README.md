# pcomp - Phase 1-2 scaffold

| Day | Where |
|-----|-------|
| 1 | `include/pc/thread_pool.hpp` (execution model, interface only), `scripts/make_samples.py` -> `datasets/manifest.json`, `scripts/env_sheet.sh`, `include/pc/bench.hpp` (timer/CPU capture) |
| 2 | `docs/ORDERING.md`, `include/pc/block.hpp` (BlockJob/BlockResult/ReorderBuffer/sorted_files), JSONL serialization in `bench.hpp` |
| 3 | `src/block_loop.cpp` (read/process/write timing), `bench/bench_main.cpp` |
| 4 | Worker sweep `--workers 1,2,4,max`; N>1 recorded as `skipped_parallel_not_implemented` until `kParallelPathSafe = true` |
| 5 | `process_ms` in each record, compress/decompress as separate phases, `scripts/run_bench.sh` |

Run: `./scripts/run_bench.sh [reps] [block_size]` -> `results/env.md` + `results/bench_<stamp>.jsonl`
