# UATC-007: Day 3 — Block Slicing

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-01, FR-02, FR-04

## Description

Implement block slicing for byte buffers and verify exact reconstruction of the input stream.

## Tasks

- [ ] Create `include/core/block_slicer.hpp` — `slice_blocks(data, block_size)` function
- [ ] Verify exact reconstruction (concatenating blocks reproduces original input)
- [ ] Handle edge cases (empty data, non-multiple sizes)

## Acceptance Criteria

- Slicing produces blocks of at most block_size bytes
- Reassembly of block data is byte-identical to original input
- Empty input produces zero blocks
