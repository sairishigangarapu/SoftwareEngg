# UATC-021: Day 7 - RleTransform

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-06, FR-08

## Description

Implement RleTransform: run-length encoding as (count, value) pairs.

## Tasks

- [ ] Create `include/core/rle_transform.hpp` / `src/core/rle_transform.cpp`
- [ ] Self-delimiting: each run occupies exactly 2 bytes, count in 1..255
- [ ] decode rejects odd-length input and a zero run length
- [ ] Transform ID = `TransformId::RLE` (4)

## Acceptance Criteria

- decode(encode(x)) == x for all inputs, including empty
- Malformed input is rejected with a clear error
- Much cheaper than raw on long runs
