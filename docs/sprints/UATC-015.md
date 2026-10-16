# UATC-015: Day 5 — CodecResult & Codec IDs

**Sprint**: Sprint 5 — Entropy Coding  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Define CodecResult and codec IDs for the entropy coding stage.

## Tasks

- [ ] Create `include/core/codec_result.hpp` — CodecResult struct
- [ ] Reuse CodecId enum from `archive/archive_format.hpp`
- [ ] Fields: encoded_data (vector<uint8_t>), codec_id, original_size

## Acceptance Criteria

- CodecResult holds encoded bytes + codec metadata
- CodecId enum is stable and shared with archive layer
