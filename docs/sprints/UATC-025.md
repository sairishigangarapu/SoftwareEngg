# UATC-025: Day 9 - TransformSelector

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-06, FR-08

## Description

Implement the TransformSelector: pick the cheapest transform for a block, break
ties by the lowest transform id, and store the block unchanged when no
transform pays off.

## Tasks

- [ ] Create `include/core/transform_selector.hpp` / `src/core/transform_selector.cpp`
- [ ] Score every registered transform with `estimateCost`
- [ ] Lowest cost wins; ties resolve to the lowest transform id (ADR-05)
- [ ] Store-vs-compress decision: the coded form plus metadata must be smaller
- [ ] Selection is deterministic and independent of the worker count

## Acceptance Criteria

- A numeric ramp selects Delta
- A long run selects RLE
- Random data is stored unchanged
- The same input always yields the same transform and cost
