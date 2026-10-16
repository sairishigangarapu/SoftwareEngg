# UATC-013: Day 4 — Transform Registry

**Sprint**: Sprint 4 — Transform Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Expose a transform registry so transforms can be added without changing compressor logic.

## Tasks

- [ ] Create `include/core/registry.hpp` — TransformRegistry class
- [ ] Register transforms by TransformId
- [ ] Lookup by ID returns Transform*
- [ ] NoneTransform registered with ID 0

## Acceptance Criteria

- Registry allows adding transforms without modifying compressor code
- Lookup by ID works correctly
- NoneTransform is registered at startup
