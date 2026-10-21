# UATC-026: Day 9 - Core ↔ archive integration test

**Sprint**: Sprint 7 (Adaptive Selection)  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1  
**Requirements**: FR-02, FR-03, FR-09

## Description

Add an integration test that exercises the compression core and the archive
layer together across the full transform set.

## Tasks

- [ ] Create `tests/core/archive_integration_test.cpp`
- [ ] Every transform survives a full archive round-trip (raw and baseline codec)
- [ ] Mixed per-block transforms in one archive round-trip
- [ ] The adaptive pipeline produces an archive the archive layer reads back
- [ ] A corrupted payload is still detected

## Acceptance Criteria

- All round-trips are byte-for-byte identical
- Corruption detection still works after the transform set grew
