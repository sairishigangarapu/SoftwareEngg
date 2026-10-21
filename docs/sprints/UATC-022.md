# UATC-022: Day 7 - Transform factory

**Sprint**: Sprint 6 (Extended Transform Set)  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1  
**Requirements**: NFR Maintainability

## Description

Add a transform factory as the single source of truth for `TransformId` →
implementation, and a shared cost estimator.

## Tasks

- [ ] Create `include/core/transform_factory.hpp` / `src/core/transform_factory.cpp`
- [ ] `make_transform(id)` returns the implementation or nullptr
- [ ] `candidate_transform_ids()` lists the v1 candidate set in stable id order
- [ ] `make_default_registry()` returns a registry holding the whole set
- [ ] The archive layer delegates to the factory (one implementation per transform)

## Acceptance Criteria

- Adding a transform touches only the factory and its own class
- The archive layer accepts every registered transform without being rewritten
- The cost estimate is deterministic and allocation-free
