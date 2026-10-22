# UATC-029: Use CRC32 per block and SHA-256 per file/archive

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: S Chakshu (archive layer) with Sai Rishi (core)  
**Priority**: P1  
**Requirements**: FR-09, FR-12, NFR-03  
**GitHub**: #41

## Description

Bring the integrity data in line with the SAD: CRC32 per block, SHA-256 per
file and per archive, plus the archive footer.

## Background

SAD section 3.6 and the archive format in SAD 4.1 specify:

- **CRC32** per block
- **SHA-256** per file and per archive, in the Integrity Data region
- An archive-level SHA-256 over all preceding regions, followed by a footer carrying the header offset and the magic

The implementation currently uses **FNV-1a 32-bit** for both the per-block
checksum and the archive integrity field (`calculate_checksum` in
`src/archive/archive_writer.cpp` and `src/archive/archive_reader.cpp`). FNV-1a
is not a CRC and is much weaker against accidental corruption.

## Tasks

- [ ] Replace the per-block checksum with a standard CRC32 (IEEE 802.3 polynomial, reflected). `BlockRecord::checksum` is already a uint32, so the format does not change
- [ ] Add a SHA-256 over the file contents, stored in the file record or in a dedicated integrity region
- [ ] Add the archive-level SHA-256 and the footer described in SAD 4.1, so the reader can find the header from the end of the file
- [ ] Distinguish the error codes: `E_BLOCK_CHECKSUM` for a per-block mismatch, `E_ARCHIVE_CHECKSUM` for the archive-level one
- [ ] Verify the archive checksum before trusting the block index
- [ ] Add the corruption test cases: TC-INT-01 (flip a byte in a payload) and TC-INT-02 (corrupt the archive-level checksum or footer)

## Acceptance Criteria

- Per-block checksums are CRC32 and are verified on extraction
- A file-level SHA-256 is stored and verified
- An archive-level SHA-256 and footer are present, and corrupting either is reported as `E_ARCHIVE_CHECKSUM`
- Corruption is always detected and never produces silently wrong output
