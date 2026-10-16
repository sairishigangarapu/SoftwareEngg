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
  3. [Transform Engine] → for each candidate Ti:
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
| Web Frontend | Manasa | (external) |

## Shared Types (from Chakshu's archive layer)

- `TransformId` — stable enum: None=0, RLE=1, Delta=2 (`archive/archive_format.hpp`)
- `CodecId` — stable enum: None=0, Baseline=1 (`archive/archive_format.hpp`)
- `BlockRecord::transform_parameter` — uint16_t for transform params

## Data Structures

- **Block**: `block_id`, `offset`, `data` (reused from `archive/block.hpp`)
- **BlockStats**: `size`, `unique_byte_count`, `histogram[256]`
- **TransformResult**: `data` (ByteBuffer), `params` (uint32_t), `is_valid`
- **CodecResult**: `data` (ByteBuffer), `codec_id` (CodecId), `original_size`
