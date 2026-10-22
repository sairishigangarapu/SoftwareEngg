# UATC-002: Day 1 — Pipeline Documentation

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-04, FR-05, FR-06, FR-07

## Description

Document the intended compression pipeline: block → stats/probe → transform → entropy codec → cost decision.

## Tasks

- [ ] Create `docs/core/pipeline.md` with pipeline flow and data structures
- [ ] Define TransformResult (bytes + params)
- [ ] Define the cost decision flow (estimate cost per transform, pick lowest, store decision)

## Acceptance Criteria

- Pipeline document is written and reviewed
- Data structures for each stage are defined
- Cost decision logic is documented
