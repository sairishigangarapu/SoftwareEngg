# Deterministic ordering rules (Day 2)

Goal: serial and parallel runs produce **byte-identical** archives.

## Rules
1. **File order**: relative path, `/` separators, bytewise (unsigned) compare. Not directory order, not mtime.
2. **Block order**: fixed-size blocks by offset. Block size is a config value, never derived from thread count.
3. **Sequence numbers**: only the single producer assigns `seq` (0,1,2,... across the archive).
4. **Output order**: the single writer emits results by `seq` through the `ReorderBuffer`.
5. **Codec purity**: `compress(block)` depends only on block bytes + fixed params.

## Must NEVER depend on thread completion order
- Position of any block/file in the archive
- Contents of any compressed block (no shared dictionaries/state that mutate during the run)
- Any header, index, or offset table (computed from seq order after the fact)
- Checksums (computed per block or in seq order)
- Timestamps, thread ids, worker counts written into the archive

## Allowed to vary
- Timing numbers, which worker ran which block, queue interleaving.

## Verification
Compress with 1 worker and N workers; `sha256sum` both archives must match.
