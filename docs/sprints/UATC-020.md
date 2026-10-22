# UATC-020: Day 7 - ByteShuffleTransform

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-06, FR-08

## Description

Implement ByteShuffleTransform: transpose the byte matrix so that equal byte
positions form contiguous streams.

## Tasks

- [ ] Create `include/core/byte_shuffle_transform.hpp` / `src/core/byte_shuffle_transform.cpp`
- [ ] Fixed stride constant `kByteShuffleStride = 16`, so no metadata is needed
- [ ] encode: write column 0 of every row, then column 1, and so on
- [ ] decode: the inverse transpose
- [ ] Transform ID = `TransformId::ByteShuffle` (3)

## Acceptance Criteria

- decode(encode(x)) == x for all inputs, including empty
- Round-trips correctly for every length around the stride boundary
- Cheaper than raw on structured multi-byte data
