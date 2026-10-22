# UATC-010: Day 4 — NoneTransform

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-06, FR-08

## Description

Implement NoneTransform: encode returns original bytes and decode returns original bytes.

## Tasks

- [ ] Create `include/core/none_transform.hpp` — NoneTransform class (returns input unchanged)
- [ ] Implement encode (return original) and decode (return original)
- [ ] Transform ID = 0 (None)

## Acceptance Criteria

- encode(x) == x for any input x (including empty)
- decode(encode(x)) == x
- Transform ID matches TransformId::None from archive_format.hpp
