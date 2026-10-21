# UATC Sprint Plan — Rishi_implementation

**Author**: Sai Rishi Gangarapu (Team Lead)  
**Branch**: `Rishi_implementation` (Days 1-5), `Rishi_implementation_v2` (Days 6-9)  
**Scope**: Compression core + adaptive transform engine (FR-04, FR-05, FR-06, FR-07)  
**Sprint duration**: 1 day each (Day 1 through Day 9)

## Sprints

### Sprint 1 — Core Skeleton (Day 1)
**Goal**: Establish the C++17/CMake core skeleton and define compression-core module boundaries.
- [UATC-001] Create core skeleton: Transform interface & Block data structure
- [UATC-002] Document the compression pipeline: block → stats/probe → transform → entropy codec → cost decision
- [UATC-003] Create first unit-test target in CMake with trivial build-and-run test

### Sprint 2 — Data Structures (Day 2)
**Goal**: Finalize data structures and IDs for the MVP.
- [UATC-004] Finalize Block and BlockStats structures (size, unique-byte count, byte-frequency histogram)
- [UATC-005] Choose and document initial block size for MVP; make it configurable
- [UATC-006] Review metadata fields with Chakshu before serialization

### Sprint 3 — Block Operations (Day 3)
**Goal**: Implement block slicing and statistics computation with edge-case tests.
- [UATC-007] Implement block slicing for byte buffers; verify exact stream reconstruction
- [UATC-008] Implement BlockStats: size, unique-byte count, byte-frequency histogram
- [UATC-009] Add tests for empty input, smaller-than-block, exact-block-size, non-multiple sizes

### Sprint 4 — Transform Engine (Day 4)
**Team Goal**: Prove the first reversible transform and raw archive path.
- [UATC-010] Implement NoneTransform (encode returns original, decode returns original)
- [UATC-011] Define TransformResult (transformed bytes + transform metadata)
- [UATC-012] Write round-trip tests: decode(encode(x)) == x for random and hand-crafted blocks
- [UATC-013] Expose TransformRegistry (transforms addable without changing compressor logic)

### Sprint 5 — Entropy Coding (Day 5)
**Goal**: Integrate baseline entropy coder behind Codec interface and benchmark.
- [UATC-014] Integrate baseline lossless entropy-coding (PackBits-style) behind Codec interface
- [UATC-015] Define CodecResult and codec IDs
- [UATC-016] Implement encode/decode tests for random data and repetitive data
- [UATC-017] Measure encoded size for None→codec on the initial dataset

### Sprint 6 — Extended Transform Set (Days 6-7)
**Goal**: Complete the v1 candidate set of five reversible transforms.
- [UATC-018] Implement DeltaTransform (byte-wise differencing) with cost estimate
- [UATC-019] Implement XorTransform (byte-wise XOR against previous byte)
- [UATC-020] Implement ByteShuffleTransform (transpose the byte matrix)
- [UATC-021] Implement RleTransform (run-length encoding as count/value pairs)
- [UATC-022] Add transform factory as the single source of truth for TransformId → implementation
- [UATC-023] Add shared cost estimator (fast, deterministic coded-size approximation)

### Sprint 7 — Adaptive Selection (Days 8-9)
**Goal**: Close the loop: probe, select the cheapest transform, store when it does not pay.
- [UATC-024] Implement CompressibilityProbe (cheap sampled estimate, FR-05)
- [UATC-025] Implement TransformSelector (lowest estimated cost, ties to lowest id, FR-06/FR-08)
- [UATC-026] Add core ↔ archive integration test across the full transform set
- [UATC-027] Align TransformId values with the SAD and verify Chakshu's archive layer

### Sprint 8 — Integration, Hardening and Demo
**Goal**: Make the prototype demo-able end to end, and close the remaining gaps against the SAD.
- [UATC-028] Implement the thread pool — FR-10 parallel block execution (Sampada, open)
- [UATC-029] Use CRC32 per block and SHA-256 per file/archive (Chakshu + Rishi, open)
- [UATC-030] Reject path traversal when the archive is extracted to disk (Chakshu + Manasa, open)
- [UATC-031] CLI compress/extract, Compression Manager, error codes and demo runbook (Rishi + Manasa, delivered)
