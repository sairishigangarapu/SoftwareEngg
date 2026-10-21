# UATC-027: Day 9 - Transform ID alignment & Chakshu verification

**Sprint**: Sprint 7 (Adaptive Selection)  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P0  
**Requirements**: FR-02, FR-06, FR-12

## Description

Align the `TransformId` values with the SAD and verify Chakshu's archive layer
against the extended transform set.

## Background

The SAD fixes the transform ids: `0 = None, 1 = Delta, 2 = XOR, 3 =
Byte-shuffle, 4 = RLE, 5 = reserved`. The archive layer had been using
`RLE = 1, Delta = 2`, which conflicts with the SAD and would have produced
incompatible archives.

## Tasks

- [ ] Update `TransformId` in `include/archive/archive_format.hpp` to match the SAD
- [ ] Add `Xor` and `ByteShuffle` to the enum, keep `Reserved` for the stretch goal
- [ ] Replace the archive reader's hard-coded transform whitelist with the
      core's candidate set, so it cannot drift again
- [ ] Extend Chakshu's archive reader and transform/codec tests to the full set
- [ ] Verify the archive layer still rejects unsupported transforms, codecs,
      versions, bad offsets and corrupted payloads

## Acceptance Criteria

- Transform ids match the SAD table
- The archive layer accepts every transform the core registers
- Unsupported transform ids are still rejected
- All of Chakshu's tests pass with the extended transform set
