# UATC-018: Day 6 - DeltaTransform

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-06, FR-08

## Description

Implement DeltaTransform: byte-wise differencing against the previous byte.

## Tasks

- [ ] Create `include/core/delta_transform.hpp` / `src/core/delta_transform.cpp`
- [ ] encode: `out[0] = in[0]`, `out[i] = in[i] - in[i-1]`
- [ ] decode: `out[0] = in[0]`, `out[i] = out[i-1] + in[i]`
- [ ] `estimateCost` counts the non-zero deltas that survive coding
- [ ] Transform ID = `TransformId::Delta` (1)

## Acceptance Criteria

- decode(encode(x)) == x for all inputs, including empty
- Cheaper than raw on slowly-changing data
- Transform ID matches the archive format
