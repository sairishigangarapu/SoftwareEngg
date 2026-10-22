# UATC-004: Day 2 — Finalize Block & BlockStats Structures

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-04, FR-05

## Description

Finalize Block and BlockStats structures. Define transform IDs and codec IDs as stable enums. Review metadata fields with Chakshu.

## Tasks

- [ ] Create `include/core/block_stats.hpp` — BlockStats struct: size, unique_byte_count, histogram[256]
- [ ] Define TransformId and CodecId as stable enums (reuse from `archive/archive_format.hpp`)
- [ ] Make block size configurable (not hard-coded)
- [ ] Cross-reference metadata fields with Chakshu's archive_format.hpp

## Acceptance Criteria

- BlockStats computes size, unique bytes, byte-frequency histogram
- Block size is a configurable parameter (default 1 MiB)
- Transform/codec ID enums are stable and never reused
