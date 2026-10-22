# UATC-009: Day 3 — Edge Case Tests

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Add tests for empty input, smaller-than-block input, exact-block-size input and non-multiple sizes.

## Tasks

- [ ] Create `tests/core/block_slice_test.cpp`
- [ ] Test: empty input → zero blocks
- [ ] Test: input smaller than block_size → one block with all data
- [ ] Test: input exactly equal to block_size → one block
- [ ] Test: input non-multiple of block_size → multiple blocks with correct sizes

## Acceptance Criteria

- All edge-case tests pass
- Reconstruction matches original for every case
