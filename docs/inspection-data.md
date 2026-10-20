# Archive Inspection Data

This is what `uatc info` and the web UI show for an archive.
Owner: Manasa. Producers: Sai Rishi (transforms), Chakshu (archive layer).

## Fields

Archive-level:
- archive: file name
- version: archive format version
- files: number of files
- blocks: total number of blocks
- original_bytes: total size before compression
- stored_bytes: total size inside the archive
- ratio: original_bytes / stored_bytes

Block decisions:
- transforms: how many blocks used each transform
  (none, rle, delta, xor, shuffle)
- codecs: how many blocks used each codec
- stored_raw: blocks kept as-is because compression didn't help

Timing:
- time_ms: total time for the operation
- throughput_mb_s: original_bytes / time

## Example

{
  "archive": "sample.uatc",
  "version": 1,
  "files": 12,
  "blocks": 340,
  "original_bytes": 10485760,
  "stored_bytes": 4194304,
  "ratio": 2.5,
  "transforms": { "none": 120, "rle": 40, "delta": 150, "xor": 20, "shuffle": 10 },
  "codecs": { "raw": 120, "baseline": 220 },
  "stored_raw": 120,
  "time_ms": 820,
  "throughput_mb_s": 12.2
}

## Open questions for teammates
- Chakshu: can the block index give us transform ID, codec ID,
  original length and stored length per block? (needed for the counts above)
- Sai Rishi: are the transform IDs stable integers? We need names for display.
- Timing is only known at compress time. Do we store it in the archive,
  or does `info` skip it? (my suggestion: skip it in `info`, show it only after compress/benchmark)
