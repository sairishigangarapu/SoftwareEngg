# UATC Compression Core Pipeline

**Owner**: Sai Rishi Gangarapu (Team Lead)  
**Module**: `include/core/`, `src/core/` (namespace `uatc::core`)

## Per-Block Compression Pipeline

```
File input → [File Analyzer (Chakshu)] → file list (sorted)
                                    ↓
Each file: [Block Slicer] → blocks of ≤ block_size bytes
                                    ↓
For each block B:
  1. [BlockStats] → entropy estimate, unique bytes, histogram
  2. [Compressibility Probe] → is B worth compressing?
     └─ no  → store raw (None transform + raw codec)
  3. [Transform Selector] → for each candidate Ti:
     └─ Ti.estimateCost(B) → pick T* with lowest cost (ties → lowest TransformId)
  4. [Entropy Coder] → encode T*(B) using Codec
  5. [Cost Decision] → if coded + metadata ≥ original size → store raw
  6. [Block Record] → (offset, lengths, transform ID, codec ID, params, CRC32)

Output: blocks emitted in seq order (deterministic, ADR-05)
```

## Module Boundaries

| Module | Owner | Files |
|--------|-------|-------|
| Compression Core (this) | Sai Rishi | `include/core/`, `src/core/` |
| Archive Layer | S Chakshu | `include/archive/`, `src/archive/` |
| Filesystem Layer | S Chakshu | `include/filesystem/`, `src/filesystem/` |
| Benchmark Scaffold | Sampada | `pcomp/` |
| Web Frontend | Manasa | `web/` |

The archive layer delegates to the core's transform factory, so each
transform has exactly one implementation and the archive layer never needs
editing when the candidate set grows.

## Shared Types (from Chakshu's archive layer)

- `TransformId` — stable enum, per the SAD: None=0, Delta=1, Xor=2, ByteShuffle=3, RLE=4, Reserved=5
- `CodecId` — stable enum: None=0, Baseline=1
- `BlockRecord::transform_parameter` — uint16_t for transform params

## Data Structures

- **Block**: `block_id`, `offset`, `data` (reused from `archive/block.hpp`)
- **BlockStats**: `size`, `unique_byte_count`, `histogram[256]`
- **TransformResult**: `data` (ByteBuffer), `params` (uint32_t), `is_valid`
- **CodecResult**: `data` (ByteBuffer), `codec_id` (CodecId), `original_size`
- **ProbeResult**: `compressible`, `entropy_bits_per_byte`, `estimated_ratio`, `sampled_bytes`
- **SelectionResult**: `transform_id`, `transformed`, `estimated_cost`, `stored_raw`

## Transform Set (v1 candidate set)

| ID | Transform | Best for |
|----|-----------|----------|
| 0 | None | incompressible data, store decision |
| 1 | Delta | slowly changing data, sorted counters |
| 2 | XOR | repeating patterns, high-bit noise |
| 3 | ByteShuffle | structured multi-byte data (int32 tables) |
| 4 | RLE | long runs of identical bytes |
