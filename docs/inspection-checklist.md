# Archive Inspection Checklist

Owner: Manasa K (FR-11, FR-12). Day 2.
Source: SAD 4.1 (archive format), SAD 4.3.1, Synopsis section 11, STP Appendix A.
Extends: `docs/inspection-data.md` (Day 1), which says *what* `info` shows.
This document says *what each command must look at and check*, region by
region, so Chakshu (archive layer) and Manasa (inspector/verifier) agree
before any serialization code is written.

## 1. Commands in one line

| Command | Reads | Checks | Writes |
| --- | --- | --- | --- |
| `info` | Magic + Version, Global Metadata, File Table, Block Index | Only what is needed to read them safely (magic, version, header checksum, bounds). Never decodes payloads. | nothing |
| `verify` | Whole archive | Everything in section 2, stops at the first failure | nothing (VER-01: nothing written to disk) |
| `extract` | Whole archive | Same checks as `verify`, per block, before bytes are written | output files only |

## 2. Checklist by archive region

Legend: **I** = shown by `info`. **V** = checked by `verify`.

| # | Region | Item | I | V | What to check | If it fails |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Magic + Version | Magic is `UATC` (4 bytes) | | V | first 4 bytes match | E_MALFORMED_ARCHIVE |
| 2 | Magic + Version | Format version (major u16, minor u16) | I | V | major is supported; minor may be newer | E_UNSUPPORTED_VERSION |
| 3 | Global Metadata | Header checksum | | V | recomputed value equals stored value | E_MALFORMED_ARCHIVE (PROPOSED, see error-categories.md section 5) |
| 4 | Global Metadata | File count, block count | I | V | within configured limits, consistent with table sizes | E_MALFORMED_ARCHIVE (TC-SEC-02) |
| 5 | Global Metadata | Offsets of the following regions | | V | inside the file, in order, not overlapping | E_MALFORMED_ARCHIVE |
| 6 | Global Metadata | Flags, creation time, block-size policy | optional | V | known flags only | E_MALFORMED_ARCHIVE |
| 7 | File Table | Relative path (UTF-8, `/` separators) | optional | V | not absolute, no `..` segment, stays inside destination | E_PATH_TRAVERSAL |
| 8 | File Table | Original size per file | I (sum) | V | sum of file sizes = sum of block original lengths | E_MALFORMED_ARCHIVE |
| 9 | File Table | First block index + block count | | V | range lies inside the Block Index | E_MALFORMED_ARCHIVE |
| 10 | File Table | SHA-256 per file | | V | recomputed over the file bytes equals stored value | E_ARCHIVE_CHECKSUM |
| 11 | Block Index | Original length (u32), stored length (u32) | I (sums) | V | no impossible lengths; stored length <= declared limit | E_MALFORMED_ARCHIVE (TC-SEC-02) |
| 12 | Block Index | Payload offset (u64) | | V | `offset + stored length` is inside the file | E_MISSING_BLOCK |
| 13 | Block Index | Transform ID (u8) | I (usage) | V | known ID (0-4; 5 reserved) | PROPOSED: open question, see error-categories.md section 5 |
| 14 | Block Index | Codec ID (u8) | I (usage) | V | known ID (0 raw, 1 Huffman) | same as row 13 |
| 15 | Block Index | Raw block rule | I (stored count) | V | codec 0 means stored length = original length | E_MALFORMED_ARCHIVE |
| 16 | Block Index | CRC32 per block | | V | CRC32 of the block (after decode and inverse transform) equals stored value | E_BLOCK_CHECKSUM (report block index) |
| 17 | Block Payloads | Every block is present and readable | | V | read does not hit end of file | E_MISSING_BLOCK |
| 18 | Integrity Data | Footer (header offset + magic) | | V | footer present and points at the header | E_MALFORMED_ARCHIVE (PROPOSED) |
| 19 | Integrity Data | Archive SHA-256 | | V | SHA-256 over all preceding regions equals stored value | E_ARCHIVE_CHECKSUM |
| 20 | File system | Archive file size | I | | from the file system, not from the archive | n/a |

## 3. Order of checks

`verify` and `extract` run the checks in this order and **stop at the first
failure**, so one kind of damage gives one distinct code (Day 15 goal):

1. Magic (row 1)
2. Version (row 2)
3. Header checksum, limits and region offsets (rows 3-6)
4. File Table and Block Index consistency, paths (rows 7-9, 11, 13-15)
5. Payload bounds (rows 12, 17)
6. Per-block CRC32, in block order (row 16)
7. Per-file SHA-256 and footer / archive SHA-256 (rows 10, 18, 19)

Why this order: a flipped payload byte would also break the archive SHA-256,
but the block CRC32 is found first, so the user gets E_BLOCK_CHECKSUM with a
block index (TC-INT-01), not a vaguer archive-level error.
`info` runs steps 1-4 only.

## 4. What `info` still has to reject

`info` never decodes payloads, but it must still refuse an archive it cannot
read safely: bad magic, unsupported version, failed header checksum, offsets
or counts outside the limits. These return the same error codes as `verify`.

## 5. Gaps found in `inspection-data.md` (Day 1)

Not fatal, listed so the team can decide:

- Section 1 has no field for creation time, block-size policy or flags
  (they exist in Global Metadata). Suggest an optional `--verbose` output.
- Section 1 has no per-file list. The File Table has it; useful for the
  Info screen, optional.
- `info` JSON has no `formatVersion` string form; the CLI prints "format 1.0"
  while the API returns `{major, minor}`. Fine, but the UI should format it.

## 6. Test mapping

| Checklist rows | Test cases |
| --- | --- |
| 1-3, 18 | TC-ERR-03, TC-ERR-01 |
| 2 | TC-ERR-02 |
| 4, 11 | TC-SEC-02 |
| 7 | TC-SEC-01 |
| 12, 17 | TC-ERR-01 |
| 16 | TC-INT-01 |
| 10, 19 | TC-INT-02 |
| `info` output | TC-INFO-01 |
| `verify` on intact archive | TC-VER-01 |
