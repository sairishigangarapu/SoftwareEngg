# UATC-001: Day 1 — Core Skeleton & Module Boundaries

**Sprint**: Sprint 1 — Core Skeleton  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-04, FR-05, FR-06, FR-07

## Description

Create the C++17/CMake core skeleton for the compression core and define module boundaries. The compression core (`include/core/`, `src/core/`) is separate from Chakshu's archive layer (`include/archive/`, `src/archive/`) and the filesystem layer (`include/filesystem/`, `src/filesystem/`).

## Tasks

- [ ] Create `include/core/` and `src/core/` directories
- [ ] Create `include/core/transform.hpp` — abstract Transform interface (Strategy pattern)
- [ ] Create `include/core/block.hpp` — reuse Chakshu's `Block` struct from `archive/block.hpp`
- [ ] Create a trivial test that builds and runs (Day 1 validation)
- [ ] Update root `CMakeLists.txt` to include core sources

## Acceptance Criteria

- Core module compiles independently
- Transform interface is defined with virtual methods (id, estimateCost, encode, decode)
- Trivial test passes
- CMake build target created
