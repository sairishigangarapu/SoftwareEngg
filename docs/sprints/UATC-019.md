# UATC-019: Day 6 - XorTransform

**Sprint**: Sprint 6 (Extended Transform Set)  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-06, FR-08

## Description

Implement XorTransform: byte-wise XOR against the previous byte.

## Tasks

- [ ] Create `include/core/xor_transform.hpp` / `src/core/xor_transform.cpp`
- [ ] encode: `out[0] = in[0]`, `out[i] = in[i] ^ in[i-1]`
- [ ] decode: `out[0] = in[0]`, `out[i] = out[i-1] ^ in[i]`
- [ ] `estimateCost` counts the non-zero XOR results
- [ ] Transform ID = `TransformId::Xor` (2)

## Acceptance Criteria

- decode(encode(x)) == x for all inputs, including empty
- Transform ID matches the archive format
