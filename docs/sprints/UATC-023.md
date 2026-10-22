# UATC-023: Day 7 - Shared cost estimator

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1  
**Requirements**: FR-06, NFR Performance

## Description

Add a shared, fast and deterministic cost estimator that approximates the
coded size of a transformed block without actually coding it.

## Background

The SAD requires transform selection by fast cost estimate, not by
compressing every candidate in turn (ADR-04).

## Tasks

- [ ] Create `include/core/cost_estimator.hpp` / `src/core/cost_estimator.cpp`
- [ ] `estimate_coded_size()` mirrors the baseline coder's literal/run costing
- [ ] Runs of 3+ identical bytes cost 2 bytes; every other byte costs 1
- [ ] `kTransformMetadataOverhead` accounts for the per-block metadata
- [ ] The estimate never allocates and is deterministic

## Acceptance Criteria

- The estimate tracks the real coded size closely enough to rank candidates
- The estimate is allocation-free
- Identical input always yields an identical estimate
