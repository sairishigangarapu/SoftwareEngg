# UATC-008: Day 3 — Block Statistics

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-04, FR-05

## Description

Implement BlockStats skeleton: size, unique-byte count and byte-frequency histogram.

## Tasks

- [ ] Create `src/core/block_stats.cpp` — BlockStats::compute block
- [ ] Compute total size, unique byte count (0-256), 256-bin frequency histogram
- [ ] Use stats as input to compressibility probe

## Acceptance Criteria

- BlockStats computes correct unique-byte count on test data
- Histogram sums equal block size
- Empty block handled correctly
