# UATC-012: Day 4 — Round-Trip Tests

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Write round-trip tests: decode(encode(x)) == x for random and hand-crafted blocks.

## Tasks

- [ ] Create `tests/core/transform_test.cpp`
- [ ] Test with hand-crafted blocks (all zeros, ramp, mixed)
- [ ] Test with random data
- [ ] Test with empty input

## Acceptance Criteria

- All round-trip tests pass
- decode(encode(x)) == x for all test inputs
