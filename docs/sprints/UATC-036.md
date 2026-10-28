# UATC-036: Day 14 - Parallel block pipeline and determinism

**Milestone**: Milestone 3 (Days 11-17) - Hardening and Parallelism
**Assignee**: Sai Rishi Gangarapu
**Priority**: P0
**Requirements**: FR-10, ADR-05 (the pipeline half of UATC-028)

- [x] CompressionManager::compress dispatches one task per block and collects results into a pre-sized vector indexed by block id, then writes in block order. Every block's encoding depends only on its own bytes, so the archive is byte-identical for any worker count and no reorder buffer is needed
- [x] Progress is tallied on the calling thread after the barrier, in block order, so the sink is never called from a worker
- [x] Serial path below 1 MiB, so small inputs never pay thread start-up (TC-PAR-05)
- [x] --workers N in the CLI
- [x] Byte-identical archives verified for 1, 2 and 8 workers on a 4 MB mixed file (TC-PAR-01)

**Acceptance**: identical output for any worker count; no data races on the result slots.

**GitHub**: #49
