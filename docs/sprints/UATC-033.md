# UATC-033: Day 11 - CRC32 per block, replacing FNV-1a

**Milestone**: Milestone 3 (Days 11-17) - Hardening and Parallelism
**Assignee**: Sai Rishi Gangarapu
**Priority**: P0
**Requirements**: FR-09, NFR-03

Replace the FNV-1a block checksum with the CRC32 the SAD specifies.

- [x] CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320), table built at startup so no hand-maintained constant can drift
- [x] Replaces calculate_checksum in the archive writer and reader
- [x] Known-answer tests against the standard vectors: "123456789" -> 0xCBF43926, "The quick brown fox..." -> 0x414FA339

**Acceptance**: per-block checksums are CRC32 and verified on extraction; corruption is still detected.

**GitHub**: #46
