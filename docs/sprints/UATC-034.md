# UATC-034: Day 12 - SHA-256 per file and archive integrity footer

**Milestone**: Milestone 3 (Days 11-17) - Hardening and Parallelism
**Assignee**: Sai Rishi Gangarapu
**Priority**: P0
**Requirements**: FR-09, FR-12, NFR-03

Add the Integrity Data region from SAD 4.1 without a breaking change to the 32-byte file record.

- [x] SHA-256 (FIPS 180-4), one-shot and incremental, verified against the FIPS vectors, the million-'a' vector, and an every-input-length sweep from 0 to 200 that covers the padding boundaries
- [x] Archives end with a fixed 76-byte region: a 32-byte file SHA-256, a 32-byte archive SHA-256 over all preceding bytes, and a 12-byte footer with the header offset and magic
- [x] Diagnostic ordering: block checksums first, then the file checksum, then the archive checksum - the most specific diagnosis wins. A flipped payload byte is reported with its block index (TC-INT-01); a tamper that survives everything else is E_ARCHIVE_CHECKSUM (TC-INT-02)
- [x] An archive without a footer is rejected as malformed, not crashed on

**Acceptance**: all three checks verified and distinguished.

**GitHub**: #47
