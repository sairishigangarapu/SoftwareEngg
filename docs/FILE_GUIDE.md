# UATC — Complete File Guide

**Scope**: every file in the repository, what it is, and what each function does.
**Date**: 21-10-2026
**Branch state**: `main`
**File count**: 154 tracked files

---

## Architecture at a glance

```
web/ (Manasa)  ──HTTP──▶  server/ (NOT BUILT YET)
                                  │
src/cli/ ─────────────────────────┤
                                  ▼
src/core/ (Rishi)  ◀──  src/archive/ (Chakshu)  ◀──  src/filesystem/ (Chakshu)
   transforms, coder,                archive format,            file walking,
   probe, selector,                  read/write,                file reading
   compression manager               inspect, verify
```

Dependency rule: `core → archive → filesystem`. The core never depends on the CLI or the web layer.

**Ownership** (from the SAD §3.4):

| Module | Owner | Requirements |
|--------|-------|--------------|
| Compression Core (`include/core/`, `src/core/`) | Sai Rishi | FR-04, FR-05, FR-06, FR-07 |
| Archive Layer (`include/archive/`, `src/archive/`) | S Chakshu | FR-01, FR-02, FR-03, FR-09 |
| Filesystem Layer (`include/filesystem/`, `src/filesystem/`) | S Chakshu | FR-01 |
| Inspection, Verification, CLI, Frontend | Manasa K | FR-11, FR-12, FR-13, FR-14, FR-15 |
| Benchmark Scaffold (`pcomp/`) | Sampada K | FR-10, performance NFRs |

---

# 1. Root files (8)

| File | What it is |
|---|---|
| **CMakeLists.txt** | Primary build. Defines the `uatc_core` static library from all `src/` files, plus one CTest target per test binary, each with `WORKING_DIRECTORY` set to the project root so `test_data/` paths resolve. Guarded by `include(CTest)` and `BUILD_TESTING`. |
| **Makefile** | cmake-free fallback — cmake is not installed on the lab machines and there is no sudo. `CORE_SRCS` → `.o` objects → `build/libuatc_core.a`; one rule per test binary; targets `all`, `test`, `bench`, `run-bench`, `cli`, `pc_bench`, `clean`. |
| **README.md** | Project overview: what UATC is, objectives, technologies (C++17, CMake, CTest, GoogleTest, Git), build and test instructions, development workflow. |
| **.gitignore** | Excludes `build/`, `*.o`, `*.log`, `node_modules/`, `RocketMoney/`, the test binaries and `.commandcode/`. |
| **Lab1_Requirements_Engineering.md** | Lab 1 deliverable for a *different* project (Subscription & Recurring Expense Auditor) — its own FR/NFR table and a Mermaid use-case diagram. Not part of the UATC code. |
| **UATC_SAD.pdf** | Software Architecture & Design. Component ownership, ADRs 01–07, the archive format specification, sequence diagrams, API design, error codes, UX. The contract the code implements. |
| **UATC_SRS.pdf** | Software Requirements Specification. FR-01…FR-15, NFRs, archive format requirements, use cases UC-1…UC-5, team ownership split, six-phase plan of work. |
| **UATC_STP.pdf** | Software Test Plan. Test items, strategy, environment, corpus, schedule, roles, and the TC-xxx catalogue that maps onto the test files below. |

---

# 2. `include/archive/` — Chakshu's archive layer (9 headers)

| File | Contents |
|---|---|
| **archive_format.hpp** | The on-disk ABI. `UATC_MAGIC = 0x43544155`, `UATC_VERSION = 1`, region sizes (header 40 B, file record 32 B, block record 40 B). `enum TransformId {None=0, Delta=1, Xor=2, ByteShuffle=3, RLE=4, Reserved=5}` — IDs fixed by the SAD and never reused. `enum CodecId {None=0, Baseline=1}`. Structs: `ArchiveHeader` (magic, version, header_size, file_count, block_count, and the three region offsets), `FileRecord` (file_id, path_offset, path_length, original_size, first_block, block_count), `BlockRecord` (file_id, block_id, payload_offset, original_length, stored_length, transform_id, codec_id, transform_parameter, checksum). |
| **archive_binary.hpp** | `BinaryWriter` / `BinaryReader` — little-endian primitive I/O. Writer: `write_u8/u16/u32/u64`, `write_bytes`, `write_string`, `position()`. Reader: `read_u8/u16/u32/u64`, `read_bytes`, `read_string`, `position()`, `seekg()`. |
| **block.hpp** | `Block` (block_id, offset, data vector, `size()`) and `BlockSplitter::split(data, block_size)` — Chakshu's block splitter. |
| **codec.hpp** | `Codec::encode(input, CodecId)` and `Codec::decode(input, CodecId)` — the archive layer's thin wrapper over the baseline PackBits-style coder. |
| **transforms.hpp** | `Transform::encode(input, TransformId)` and `Transform::decode(input, TransformId)` — the archive layer's thin wrapper; now delegates to the core's transform factory. |
| **archive_writer.hpp** | `BlockEncoding {transform_id, codec_id}` and `CodedBlock {transform_id, codec_id, payload}` for a block that has already been transformed and coded. `ArchiveWriter::write()` in four overloads: raw, uniform transform plus codec, per-block encodings, and pre-coded blocks. |
| **archive_reader.hpp** | `ArchivedFile {relative_path, data}`. `ArchiveReader::read()` returns the bytes; `read_file()` returns the bytes **plus** the recorded relative path, so extraction can restore the original filename. |
| **archive_inspector.hpp** | `TransformUsage`, `CodecUsage`, `ArchiveInfo` (path, formatVersion, fileCount, blockCount, originalBytes, archiveBytes, ratio, storedBlocks, compressedBlocks, usage vectors). `ArchiveInspector::inspect()` reads metadata **without** decoding payloads. |
| **archive_verifier.hpp** | `VerificationResult {success, filesChecked, blocksChecked, message}`. `ArchiveVerifier::verify()` validates an archive and writes nothing to disk. |

---

# 3. `include/core/` — Rishi's compression core (22 headers)

| File | Contents |
|---|---|
| **byte_buffer.hpp** | `ByteSpan` — a non-owning view (`data` pointer plus `size`) with `begin`, `end`, `empty` and `operator[]`, constructible from a pointer and length or from a `vector`. `ByteBuffer` is an alias for `vector<uint8_t>` used for output. |
| **transform.hpp** | The Strategy-pattern `Transform` interface: `id()`, `name()`, `estimateCost(ByteSpan)`, `encode(ByteSpan) → TransformResult`, `decode(ByteSpan, params) → ByteBuffer`. Also defines `TransformResult {data, params (uint32_t), is_valid}`. |
| **config.hpp** | `kDefaultBlockSize = 1 MiB` and `Config {block_size}` — the configurable block size (the SAD requires it not to be hard-coded). |
| **block_stats.hpp** | `BlockStats {size, unique_byte_count, histogram[256]}` with `entropy_estimate()` (Shannon entropy in bits per byte) and the static `compute(ByteSpan)`. |
| **block_slicer.hpp** | Rishi's `Block {block_id, offset, data, size()}`, `slice_blocks(ByteSpan, block_size)` and `reassemble(blocks)`. Guarantees exact reconstruction of the input stream. |
| **cost_estimator.hpp** | `estimate_coded_size(data, n)` — a fast, allocation-free approximation of the baseline coder's output size. `kTransformMetadataOverhead = 1`. |
| **none_transform.hpp** | `NoneTransform` — the identity transform. `estimateCost` returns the input size. Transform ID 0. |
| **delta_transform.hpp** | `DeltaTransform` — `out[i] = in[i] - in[i-1]`. Transform ID 1. |
| **xor_transform.hpp** | `XorTransform` — `out[i] = in[i] ^ in[i-1]`. Transform ID 2. |
| **byte_shuffle_transform.hpp** | `ByteShuffleTransform` — transposes the byte matrix; `kByteShuffleStride = 16`. Transform ID 3. |
| **rle_transform.hpp** | `RleTransform` — run-length encoding as (count, value) pairs, self-delimiting. Transform ID 4. |
| **registry.hpp** | `TransformRegistry` — `register_transform(unique_ptr<Transform>)`, `get(id)`, `all()`, `ids()`. Backed by `std::map<TransformId, ...>` so iteration is in ID order, which is what makes tie-breaking to the lowest ID work. |
| **transform_factory.hpp** | `make_transform(id)`, `candidate_transform_ids()` and `make_default_registry()`. The single source of truth mapping a Transform ID to an implementation. |
| **entropy_coder.hpp** | The `EntropyCoder` interface: `id()`, `name()`, `encode(ByteSpan) → CodecResult`, `decode(ByteSpan, orig_size) → ByteBuffer`. |
| **codec_result.hpp** | `CodecResult {data, codec_id, original_size, is_valid}`. |
| **baseline_entropy_coder.hpp** | `BaselineEntropyCoder` — PackBits-style RLE: control byte 0–127 means a literal run, 129–255 means a repeat run, 128 is reserved and never emitted. |
| **compressibility_probe.hpp** | `kProbeSampleSize = 64 KiB`, `kProbeMinSize = 64`, `ProbeResult {compressible, entropy_bits_per_byte, estimated_ratio, sampled_bytes}` and `CompressibilityProbe::probe()`. |
| **transform_selector.hpp** | `SelectionResult {transform_id, transformed, estimated_cost, original_size, stored_raw}` and `TransformSelector::select(ByteSpan)`. |
| **compressor.hpp** | `CompressOptions`, `CompressResult`, `ExtractResult`, and `CompressionManager::compress()` / `extract()` — the pipeline orchestrator (SAD §3.4). |
| **planner.hpp** | `BlockPlan` (what the engine will do with one block), `AnalysisReport` (the whole-file prediction), `plan_block()` and `analyze()`. The planner drives both the "before" report and the real compression, so the two can never disagree. |
| **progress.hpp** | `BlockProgress` (one block's update) and the `ProgressSink` interface from SAD §4.3.3, plus `NullProgressSink` for callers that do not want progress. |
| **error.hpp** | `ErrorCode` (11 values), `error_code_name()` returning the `E_*` strings, `error_exit_code()` returning the SAD §4.3.4 exit codes, and the `UatcError` exception that carries a code. |

---

# 4. `include/filesystem/` — Chakshu's filesystem layer (3 headers)

| File | Contents |
|---|---|
| **file_entry.hpp** | `FileEntry {relative_path, file_size}`. |
| **file_enumerator.hpp** | `FileEnumerator::enumerate(root)` returning a sorted `vector<FileEntry>`. |
| **file_reader.hpp** | `FileReader::read(path)` returning `vector<uint8_t>`. |

---

# 5. `src/archive/` — archive layer implementations (8)

| File | What it does |
|---|---|
| **archive_binary.cpp** | 294 lines. `BinaryWriter` writes little-endian by masking and shifting byte by byte (`write_u16` writes the low byte then the high byte; `write_u32` four shifts; `write_u64` a loop of eight). Every write checks the stream state and throws on failure. `BinaryReader` mirrors it, OR-ing the bytes back together; `read_u8` throws "Unexpected end of archive" on EOF. `seekg()` clears the stream state first, so a failed seek cannot poison later reads. |
| **block.cpp** | `BlockSplitter::split()` — rejects `block_size == 0`; empty input yields zero blocks; otherwise slices at `block_size` boundaries and assigns sequential `block_id` and byte `offset` values. |
| **codec.cpp** | `Codec::encode()` — the PackBits-style baseline coder: finds runs of three or more identical bytes and emits a two-byte repeat; otherwise accumulates literal bytes up to 128. `Codec::decode()` reverses it, throwing on control byte 128, a truncated literal run and a truncated repeat run. |
| **transforms.cpp** | Now a thin adapter: `Transform::encode` and `decode` look the transform up through `core::make_transform(id)` and delegate. One implementation per transform, shared by the core and the archive layer. |
| **archive_writer.cpp** | Builds the archive. For each block it computes the FNV-1a checksum of the **original** bytes, applies the transform, then the codec. It writes header, file record, path string, block index, then the payloads, with `payload_offset` advancing by each payload's size. The per-block overload validates that the encoding count equals the block count. |
| **archive_reader.cpp** | The security-critical file. `validate_header()` checks the magic, version, header size, file count, and that the three region offsets are strictly increasing. `validate_block()` requires the transform to be one the core registers, the codec to be known, `transform_parameter` to be zero, and raw blocks to have `stored_length == original_length`. `checked_add` and `checked_multiply` reject integer overflow. It reads the file record and then each block record, validates that block IDs are sequential and that every payload lies inside the archive. Each block is decoded, its length checked against the metadata, then its checksum, then it is appended. Finally the total is checked against `original_size`. `read()` delegates to `read_file()`. |
| **archive_inspector.cpp** | `transform_name()` now pulls the name from the core factory, and `is_known_transform()` checks the core's candidate list. `inspect()` reads the header, file record and block index **only** — it never decodes payloads — and tallies stored versus compressed blocks and the per-transform and per-codec usage. |
| **archive_verifier.cpp** | 37 lines. `verify()` calls the inspector for the counts and then `ArchiveReader::read()` to force full decoding and checksum validation. It catches everything and returns it as a `VerificationResult`; it never throws. |

---

# 6. `src/cli/main.cpp` — the CLI (1 file, 481 lines)

| Function | What it does |
|---|---|
| `print_help()` | Usage for `compress`, `extract`, `info` and `verify`, plus `--block-size` and `--quiet`. |
| `transform_name(id)` | Delegates to the core factory, so the CLI's names can never drift from the engine's. |
| `codec_name(id)` | Switch over `CodecId` producing "None" or "Baseline". |
| `parse_size(value, option)` | Parses a byte count, throwing on zero or garbage. |
| `print_info(path)` | Prints the archive metadata: size, block count, ratio, stored and compressed counts, and the transform and codec breakdown. |
| `print_verify(path)` | Runs the verifier and maps the message to `E_BLOCK_CHECKSUM` (exit 13), `E_UNSUPPORTED_VERSION` (11) or `E_MALFORMED_ARCHIVE` (10). |
| `compress_file(input, output, block_size, quiet)` | Calls `CompressionManager::compress`, then prints the original and archive byte counts, the ratio, the block counts and the transform breakdown. |
| `extract_archive(archive, dir, quiet)` | Calls `CompressionManager::extract`, then prints the destination and the number of bytes written. |
| `main(argc, argv)` | Dispatches the four sub-commands, parses `--block-size` and `--quiet`, validates that the archive path exists and is a regular file, and maps exceptions to exit codes: `UatcError` to its own code's exit code, `filesystem_error` to 5, anything else to 10. |

---

# 7. `src/core/` — compression core implementations (14)

| File | What it does |
|---|---|
| **block_stats.cpp** | `BlockStats::compute()` — one pass, counting unique bytes and filling the 256-bin histogram. `entropy_estimate()` — Shannon entropy, `H = -Σ p·log2(p)`. |
| **block_slicer.cpp** | `slice_blocks()` splits at `block_size` boundaries, with the last block possibly short. `reassemble()` concatenates the block data, and is used to prove exact reconstruction. |
| **cost_estimator.cpp** | `estimate_coded_size()` walks the data counting runs: a run of three or more costs two bytes, anything else costs one byte per byte. It mirrors the baseline coder's costing without producing any output. |
| **delta_transform.cpp** | `encode()` keeps the first byte and then stores byte differences. `decode()` accumulates. `estimateCost()` counts the non-zero deltas, since zeros are cheap for the coder, plus the metadata overhead. |
| **xor_transform.cpp** | The same shape as Delta but using XOR. `estimateCost()` counts the non-zero XOR results. |
| **byte_shuffle_transform.cpp** | `row_count()` is `ceil(n/16)`. `encode()` writes column zero of every row, then column one, and so on. `decode()` is the inverse transpose. `estimateCost()` walks the shuffled order **without materialising it**, so it stays allocation-free. |
| **rle_transform.cpp** | `encode()` emits (count 1–255, value) pairs. `decode()` rejects odd-length input and a zero run length. `estimateCost()` is runs × 2 plus the overhead. |
| **baseline_entropy_coder.cpp** | The `run_length()` helper. `encode()` produces literal runs and repeat runs. `decode()` mirrors it, throwing on the reserved control byte 128, on truncated runs, and on over-large output. |
| **transform_factory.cpp** | `make_transform()` switches to the right subclass. `candidate_transform_ids()` returns the five IDs in order. `make_default_registry()` builds a registry holding all of them. |
| **compressibility_probe.cpp** | `probe()` samples the head of the block (64 KiB), computes the statistics and entropy, and estimates the coded size. **Blocks of at least 64 bytes are judged by entropy below 7.5 bits per byte**; smaller blocks fall back to the size estimate with a 25% margin, because a short block's maximum entropy is only `log2(n)` bits per byte. Empty input is never compressible. |
| **transform_selector.cpp** | `select()` scores every registered transform with `estimateCost` and keeps the strictly lowest, so ties resolve to the lowest ID as ADR-05 requires. It then applies the store decision: if the best cost is not smaller than the original size, the block is stored raw with `TransformId::None`. |
| **compressor.cpp** | `CompressionManager::compress()` validates the input, splits it into blocks, plans each one, encodes it, reports the progress, and writes the archive with per-block encodings. `extract()` reads the archive together with its recorded path, **rejects absolute paths and `..` segments** with `E_PATH_TRAVERSAL`, creates the directories and writes the file. |
| **planner.cpp** | `plan_block()` — the per-block decision: measure the entropy, ask the probe, and either pick the cheapest transform or mark the block as stored. `analyze()` — runs the planner over every block of a file to produce the "before" report, without writing anything. |
| **error.cpp** | `error_code_name()` returns `E_INVALID_ARGUMENT`, `E_NOT_FOUND`, `E_PERMISSION`, `E_IO`, `E_MALFORMED_ARCHIVE`, `E_UNSUPPORTED_VERSION`, `E_MISSING_BLOCK`, `E_BLOCK_CHECKSUM`, `E_ARCHIVE_CHECKSUM`, `E_PATH_TRAVERSAL` or `E_INTERNAL`. `error_exit_code()` returns 2, 3, 4, 5, 10, 11, 12, 13, 14, 15 or 1. |

---

# 8. `src/filesystem/` (2)

| File | What it does |
|---|---|
| **file_enumerator.cpp** | `enumerate()` throws if the root is missing or is not a directory. It walks a `recursive_directory_iterator`, skips anything that is not a regular file, records the relative path and size, and then **sorts by path** so the order is deterministic — never directory order and never mtime. |
| **file_reader.cpp** | `read()` validates that the path exists and is a regular file, opens it in binary mode, seeks to the end to get the size, allocates, reads, and throws if the read fails. It handles the empty file. |

---

# 9. `tests/` — Chakshu's and Manasa's tests (11)

| File | Coverage |
|---|---|
| **filesystem_test.cpp** | Enumerates `test_data/` and asserts that exactly the six expected files are found, then reads `hello.txt`, `empty.txt`, `binary.bin` and `"my test file.txt"` — the last one has spaces in its name, to catch path handling bugs. |
| **block_test.cpp** | `BlockSplitter::split()` — empty input yields zero blocks; smaller-than-block yields one block; the exact block size; non-multiple sizes. Verifies the offsets and IDs. |
| **archive_binary_test.cpp** | `BinaryWriter` and `BinaryReader` round-trip for u8, u16, u32, u64, raw bytes and strings. |
| **archive_writer_test.cpp** | Writes a real archive from `test_data/hello.txt`, checks that it exists and has a sensible size, and cleans up afterwards. |
| **archive_reader_test.cpp** | The large one, 390 lines. Round-trips for raw, None, RLE and Delta, and **rejects** an unsupported codec, an invalid raw stored length, an invalid transform parameter, an invalid payload offset, a corrupted raw payload, a corrupted transformed payload, an unsupported version and a truncated archive. |
| **transform_codec_test.cpp** | Round-trips all five transforms and both codecs, then all five by two combinations through the full archive, and checks that invalid RLE data is rejected. |
| **mixed_transform_test.cpp** | Four blocks in **one** archive with four different encodings — raw, codec only, delta plus codec, and RLE plus codec — which proves per-block encoding works. |
| **archive_inspector_test.cpp** | Inspects an archive and checks every field: file count, block count, original size, archive size, version, and the stored and compressed counts. It also proves that inspection does not alter the archive. |
| **archive_verifier_test.cpp** | A valid archive verifies. The test then finds the payload offset by parsing the header and the block index, flips one payload byte, and confirms that verification rejects it. |
| **integration_roundtrip_test.cpp** | The full compress, extract and compare cycle, printing the byte counts. |
| **integration_roundtrip.ps1** | The same round trip as a PowerShell script, for the Windows machines on the team. |

---

# 10. `tests/core/` — Rishi's tests (11)

| File | Coverage |
|---|---|
| **trivial_test.cpp** | Day 1. Proves the core links: the shared types are visible and `ByteSpan` and `ByteBuffer` are usable. |
| **block_slice_test.cpp** | Day 3. Empty input yields zero blocks; smaller-than-block yields one; the exact block size; a non-multiple case of ten bytes with a block size of three giving blocks of 3, 3, 3 and 1, plus a `reassemble()` equality check. Also covers `BlockStats` on known data. |
| **transform_test.cpp** | Day 4. `NoneTransform` round-trips over empty input, all zeros, a ramp, mixed content and 10 000 random bytes. Registry lookup is checked for both a registered and an unregistered transform. |
| **codec_test.cpp** | Day 5. The baseline coder over empty input, random data (round-trips and does not shrink much), repetitive data (shrinks below 20 bytes), mixed content, a single byte and a single symbol. It also carries the **core-to-archive compatibility check**: the core coder and the archive codec must produce byte-identical output and must decode each other's streams. |
| **pipeline_test.cpp** | Day 5 integration. Simulates a file, splits it, computes the statistics, runs None plus the baseline coder, verifies the reconstruction and prints the ratio. |
| **transforms_extended_test.cpp** | Days 6–7. Round-trips all five transforms over empty input, zeros, a ramp, mixed content, a single byte, structured data and 5 000 random bytes, and for **every size from 0 to 40 around the shuffle stride**. It asserts the transform ID values match the SAD, covers the factory and the default registry, and checks that the transforms are actually cheaper than raw on suitable data. |
| **probe_test.cpp** | Day 8. Empty input is not compressible; zeros are; English text is; random data is not. A large block is sampled rather than scanned in full, and a small block is probed in full. **Plus four regression cases** for the random-data bug described in §15. |
| **selector_test.cpp** | Day 9. A numeric ramp selects Delta; a long run selects RLE; random data is stored raw; an empty block resolves to the lowest ID; every selected transform round-trips; the selection is deterministic; and **different blocks of one file get different transforms**. |
| **archive_integration_test.cpp** | Day 9. Every transform through the archive layer, with the raw and the baseline codec; mixed per-block transforms in one archive; the adaptive pipeline producing an archive the layer can read; and a corrupted payload still being detected. |
| **compressor_test.cpp** | Day 10. `CompressionManager`: compress, inspect, verify, extract and compare byte for byte; an incompressible file is stored rather than squeezed; compressible data really does compress; `E_NOT_FOUND` is raised for a missing input; a zero block size is rejected; and the full error-code to exit-code table is asserted. |
| **planner_test.cpp** | Day 10. The plan and the run must agree: the same transform for every block, the same stored-versus-compressed counts, and the same byte totals. Also covers a missing input, a zero block size, and that `NullProgressSink` counts what the real sink reports. |

---

# 11. `pcomp/` — Sampada's benchmark scaffold (15)

| File | What it is |
|---|---|
| **README.md** | The **five-day workplan table** — Day 1 through Day 5, each with the file that delivers it — and the run command. This is the student handout. |
| **docs/ORDERING.md** | The determinism rules: file order by relative path (bytewise), block order by offset, only the producer assigns `seq`, only the writer emits, and codec purity. It lists what must **never** depend on thread completion order. Verification: the `sha256sum` of a one-worker archive and an N-worker archive must match. |
| **include/pc/thread_pool.hpp** | Day 1. The `Executor` interface (`submit`, `wait_idle`, `worker_count`) and the **execution model diagram** — a single producer, N workers, a reorder buffer and a single writer. `make_thread_pool()` is **declared but not implemented**. `kParallelPathSafe = false` is the gate that holds the parallel path back. |
| **include/pc/bench.hpp** | Days 1, 2 and 5. `now_ns()`, `ScopedAccum` (adds the elapsed nanoseconds to a counter on scope exit), `StageTimes {read_ns, process_ns, write_ns, wall_ns, bytes_in, bytes_out, blocks}`, `BenchRecord`, `mb_per_s()`, `json_escape()`, `env_json()` (OS, compiler, build flags, hardware thread count) and `to_json()`, the JSONL serialiser that emits `process_ms`. |
| **include/pc/block.hpp** | Day 2. `BlockJob`, `BlockResult`, `sorted_files()` (sorted by relative path) and `ReorderBuffer`, which holds results in a map and emits them strictly in `seq` order. |
| **include/pc/block_loop.hpp** | `LoopConfig {block_size, codec}`, `process_block()`, `compress_file()`, `decompress_file()` and `fnv1a_file()`. |
| **include/pc/codec.hpp** | The `Codec` interface (`name`, `compress`, `decompress`) and `RawCodec`, the placeholder that stores bytes unchanged. |
| **src/block_loop.cpp** | Day 3. `process_block()` runs the codec on one block. `compress_file()` is the single-thread loop, with `ScopedAccum` timing around the read, process and write stages and a `ReorderBuffer` for output ordering. `decompress_file()` reads (raw_size, plen) pairs and then the payload. `fnv1a_file()` is the cheap round-trip check. |
| **bench/bench_main.cpp** | The benchmark driver. It parses `--root`, `--out`, `--block-size`, `--reps` and `--workers`, expands `max` to the hardware thread count, writes the environment record, then for every file and every worker count runs compress and decompress and verifies the round trip with FNV-1a. **The Day 4 gate**: `if (w > 1 && !kParallelPathSafe)` records `skipped_parallel_not_implemented` rather than pretending to measure a parallel run. |
| **scripts/make_samples.py** | Day 1. Generates the seeded corpus: Zipf-distributed pseudo-English text, concatenated system headers, a float64 sine plus noise signal, an increasing uint32 table, a gzip file with `mtime=0`, and random bytes. It writes `manifest.json` with the sizes and SHA-256 sums. |
| **scripts/env_sheet.sh** | Day 1. Captures the CPU, RAM, OS, compiler, build flags and CPU governor into `results/env.md`, so benchmark numbers can be reproduced on other machines. It falls back gracefully when cmake is absent. |
| **scripts/run_bench.sh** | Day 5. The one-command pipeline: build (cmake or the Makefile fallback), generate the samples, write the environment sheet, run the benchmark and produce a timestamped JSONL file. |
| **datasets/manifest.json** | The corpus manifest: seed 1337, and for each file its category, path, byte count, SHA-256, description and generator. |
| **CMakeLists.txt** | A standalone build for pcomp: the `pc` static library and the `pc_bench` executable, with the Release flags pinned and recorded in `PC_BUILD_FLAGS`. |
| **.gitignore** | Excludes `build/`, `results/` and `datasets/samples/` — the generated data. |

---

# 12. `docs/` (11 documents, including this one, plus 33 sprint files)

| File | What it is |
|---|---|
| **core/pipeline.md** | Rishi's design document: the per-block pipeline, the module boundaries with owners, the shared types, the data structures and the transform set table. |
| **FILE_GUIDE.md** | This file: every file in the repository, what it is, and what each function does. |
| **DEMO.md** | The demo runbook: what to run, what each step proves, what to say, the exit-code table, and the remaining gaps. |
| **cli-spec.md** | The CLI contract — `stats`, `compress`, `extract`, `info`, `verify` and `benchmark` with their arguments, the common options, the exit codes, and example `stats`, `info` and `verify` output. |
| **api-contract.md** | The HTTP API the frontend targets: `/api/health`, `/api/fs/list`, `/api/jobs/*` and `/api/archives/info`, with the request and response shapes. Items marked PROPOSED need the team's sign-off. |
| **error-categories.md** | The full error catalogue, and how the CLI, the API and the UI each report it. |
| **inspection-data.md** | Exactly what `info` shows, and which archive region each field comes from. |
| **inspection-checklist.md** | Region by region: what `info` reads and what `verify` checks, and in what order. Written so that Chakshu and Manasa agree before any serialization code is written. |
| **progress-states.md** | The four job states — queued, running, completed, failed — the progress rules and the polling contract. |
| **wireframes.md** | Low-fidelity text wireframes for all seven screens, showing which API call each one makes. |
| **PR_DAY_02.md** | The Day 2 pull-request note: what was added and its traceability to FR-11 through FR-15. |
| **sprints/SPRINT_PLAN.md** | The sprint index, organised by milestone: **Milestone 1 (Days 1-5) — MVP Compressor** and **Milestone 2 (Days 6-10) — Adaptive Engine**, each with its goal, its exit criteria, its issue table and its current status. |
| **sprints/UATC-001…032.md** | 32 individual issue cards. UATC-001 to 017 are Milestone 1, Days 1 to 5; 018 to 027 are Milestone 2, Days 6 to 9; 028 to 030 are the open gaps (thread pool, CRC32 and SHA-256, path traversal); 031 is the demo work and 032 the cost-estimate accuracy. Each card has the milestone, the assignee, the priority, the requirements, the tasks and the acceptance criteria. |

---

# 13. `web/` — Manasa's frontend (4)

| File | What it is |
|---|---|
| **index.html** | 65 lines. Five `<section class="screen">` blocks — home, select, progress, result and error — toggled through the `hidden` attribute. The IDs are `btn-compress`, `btn-extract`, `btn-info`, `btn-verify`, `source`, `file-picker`, `destination`, `workers`, `progress-bar`, `progress-text`, `result-body` and `error-text`. The headings carry `tabindex="-1"` so focus can be moved to them for screen readers, and the status text uses `role="status"` and `role="alert"`. |
| **app.js** | 522 lines. The `USE_MOCK` flag turns mocking on by default so the page runs with no backend, and `MOCK_FAIL` reads `?fail` from the URL. `MESSAGES` maps the error codes to plain-language text, and `MOCK_RESULT` and `MOCK_INFO` hold the sample data. `show()` switches screens and moves focus. `formatBytes()`, `escapeHtml()` (the XSS guard for injected text) and `renderTable()` are the formatting helpers. `mockApi()` simulates the archive-information endpoint, a job start and the job-status polling. `api()` is the real fetch wrapper. `showError()`, `showInfo()`, `showVerification()` and `showResult()` render the four result shapes. `poll()` polls the job status, `stopPolling()` cancels it, `start()` kicks off the operation and `choose(op)` switches between compress and extract. |
| **styles.css** | 15 lines, black on white and minimal: body, header, labels, inputs, buttons, `progress`, tables and `:focus-visible` for a visible keyboard focus, plus the `.operation-grid` rule (two columns, collapsing to one on narrow screens) and a `[hidden]` guard. |
| **README.md** | An empty placeholder. |

---

# 14. `scripts/demo.sh` (1)

The one-command dry run: build, then the twenty test suites, then generate a five-file corpus, then compress and inspect each file, then extract and compare byte for byte, then verify an archive, flip one byte in it and verify again, and finally run the benchmark. Everything happens inside a `mktemp -d` scratch directory that is removed on exit.

---

# 15. `test_data/` (6)

`hello.txt` (11 B), `marks.csv` (58 B), `empty.txt` (0 B, the empty-file edge case), `binary.bin` (7 B), `data/students.txt` (70 B, in a nested directory) and `"my test file.txt"` (34 B — a filename with spaces, to catch path handling bugs).

---

# 16. Things worth knowing

## The build has no cmake dependency

cmake is not installed on the lab machines and there is no sudo, so the `Makefile` is what actually builds the project. `CMakeLists.txt` is kept correct for where cmake *is* available, and the two are kept in sync by hand. **When you add a source file, add it to both** `CORE_SRCS` in the Makefile and `add_library(uatc_core ...)` in CMakeLists.txt, plus the test target lists in both. A file in only one of them is silently not built.

## The compressibility probe is entropy-driven

The probe originally used a one-byte margin on its size estimate. On random data, chance runs of three identical bytes made that estimate come out two bytes *under* the original size, so incompressible blocks were pushed through the transform pipeline instead of being stored. The probe is now judged by Shannon entropy with a 7.5 bits-per-byte threshold, falling back to the size estimate with a 25% margin for blocks too short for the entropy estimate to mean anything. Four regression cases in `tests/core/probe_test.cpp` lock this in.

## The archive layer and the core share one transform implementation

`src/archive/transforms.cpp` is a thin adapter over `core::make_transform()`. The inspector and the CLI take their transform names and their candidate set from the same place, so the whitelist cannot drift out of step with the engine the way it did when XOR and ByteShuffle were added.

## What is still missing

- **FR-10 parallel execution.** `make_thread_pool()` is declared but never implemented and `kParallelPathSafe` is still `false`, so the benchmark records every worker count above one as `skipped_parallel_not_implemented`. Issue #40.
- **The entropy coder is still the baseline PackBits-style coder.** The interface is in place, so swapping in Huffman or rANS does not touch the compressor. This is why text compresses only slightly at the moment.
- **The checksums are FNV-1a**, not the CRC32 per block and SHA-256 per file and archive that the SAD specifies. Issue #41.
- **One file per archive.** The reader rejects any archive with a file count other than one; multi-file and directory archives are not implemented yet.
- **The HTTP API and the job manager are not built.** The frontend runs on mock data, and the contract it targets is written down in `docs/api-contract.md`.
- **No `Jenkinsfile` or `Dockerfile`**, both of which the course tooling list requires.
