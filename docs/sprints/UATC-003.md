# UATC-003: Day 1 — Unit Test Target

**Sprint**: Sprint 1 — Core Skeleton  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Create the first unit-test target in CMake and add a trivial test that builds and runs.

## Tasks

- [ ] Add `tests/core/trivial_test.cpp` — asserts core module is linked and callable
- [ ] Add CMake target `core_trivial_test` in root CMakeLists.txt
- [ ] Verify test compiles and runs

## Acceptance Criteria

- Test compiles with the core library
- Test runs and exits 0
- CTest registers the test
