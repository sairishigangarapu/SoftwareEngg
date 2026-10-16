# UATC-014: Day 5 — Entropy Coder Integration

**Sprint**: Sprint 5 — Entropy Coding  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-07

## Description

Integrate the baseline lossless entropy-coding approach (PackBits-style) behind a Codec interface.

## Tasks

- [ ] Create `include/core/entropy_coder.hpp` — abstract EntropyCoder interface
- [ ] Create `include/core/baseline_entropy_coder.hpp` — PackBits-style baseline coder
- [ ] Define encode/decode with CodecId

## Acceptance Criteria

- EntropyCoder interface is abstract with virtual methods
- Baseline coder round-trips correctly
- Compatible with Chakshu's archive codec
