# UATC-005: Day 2 — Block Size Configuration

**Milestone**: Milestone 1 (Days 1-5) - MVP Compressor  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1

## Description

Choose and document initial block size for MVP; make it configurable rather than hard-coded.

## Tasks

- [ ] Define `kDefaultBlockSize = 1 << 20` (1 MiB) as a named constant
- [ ] Make block size overridable via config (compile-time or runtime)
- [ ] Document rationale for 1 MiB (balance memory vs. parallelism granularity)

## Acceptance Criteria

- Block size is a named constant, not a magic number
- Can be overridden via function parameter or config struct
