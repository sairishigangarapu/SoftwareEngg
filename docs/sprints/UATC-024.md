# UATC-024: Day 8 - CompressibilityProbe

**Sprint**: Sprint 7 (Adaptive Selection)  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-05

## Description

Implement the CompressibilityProbe: a cheap estimate, taken on a sample, of
whether a block is worth pushing through the compression pipeline.

## Tasks

- [ ] Create `include/core/compressibility_probe.hpp` / `src/core/compressibility_probe.cpp`
- [ ] Sample the head of the block (`kProbeSampleSize` = 64 KiB)
- [ ] Report Shannon entropy, estimated ratio and the sampled size
- [ ] A block is compressible when its estimated coded size beats the original
- [ ] Empty blocks are never compressible

## Acceptance Criteria

- Text and zero-filled blocks are flagged compressible
- Random and already-compressed blocks are not
- The probe never performs the full compression work
