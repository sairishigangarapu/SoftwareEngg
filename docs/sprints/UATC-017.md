# UATC-017: Day 5 — Benchmark Encoded Size

**Sprint**: Sprint 5 — Entropy Coding  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Measure encoded size for None→codec on the initial dataset (pcomp samples).

## Tasks

- [ ] Run Baseline entropy coder on pcomp sample data (text, numeric, compressed, random)
- [ ] Report encoded sizes and compression ratios
- [ ] Compare None (store) vs Baseline codec sizes

## Acceptance Criteria

- Encoded sizes measured for all sample categories
- Report shows compression ratio per category
- Random/compressed data does not shrink (store decision would trigger)
