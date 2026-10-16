# UATC-016: Day 5 — Entropy Coder Tests

**Sprint**: Sprint 5 — Entropy Coding  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Implement encode/decode tests for random data and repetitive data.

## Tasks

- [ ] Create `tests/core/codec_test.cpp`
- [ ] Test with random data (round-trip)
- [ ] Test with repetitive data (compression effectiveness)
- [ ] Test empty input

## Acceptance Criteria

- Random data round-trips correctly
- Repetitive data is compressed (output smaller than input)
- Empty input handled correctly
