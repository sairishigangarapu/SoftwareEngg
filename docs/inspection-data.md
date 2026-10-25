# Archive Inspection Data

Owner: Manasa K (FR-11, FR-12). Source: SRS 3.1, SAD 3.4, 4.1, 4.3.
`info` reads the .uatc index only. It never decodes block payloads.

## 1. What `info` shows (read from the archive)

| Field | Meaning | Comes from |
| --- | --- | --- |
| path | archive file path | request |
| formatVersion | {major, minor} | Magic + Version |
| fileCount | files in the archive | Global Metadata |
| blockCount | blocks in the archive | Global Metadata |
| originalBytes | sum of original file sizes | File Table |
| archiveBytes | size of the .uatc file | file system |
| ratio | originalBytes / archiveBytes | computed |
| storedBlocks | blocks kept unchanged (codec 0 = raw) | Block Index |
| compressedBlocks | blockCount - storedBlocks | computed |
| transformUsage | per transform: id, name, blocks | Block Index |
| codecUsage | per codec: id, name, blocks | Block Index |

Transform IDs (SAD 4.1, stable, never reused):
0 None, 1 Delta, 2 XOR, 3 Byte-shuffle, 4 RLE, 5 reserved.
Codec IDs: 0 raw, 1 Huffman.

Note: a block with transform 0 (None) can still be Huffman coded.
"Stored as-is" means codec 0, not transform 0.

## 2. What a finished job reports (not stored in the archive)

These exist only at the end of compress / extract / verify, so `info`
cannot show them. They come from the job result.

| Field | Meaning |
| --- | --- |
| elapsedMs | time the operation took |
| throughputMBps | originalBytes / elapsed time |
| blocks, stored | block counts for this run |
| transformUsage | same shape as above |

Later additions (Day 9): blocks probed vs compressed vs stored. The
probe decision is not saved in the archive, so this is job-result only.

## 3. Example `info` output (JSON shape used by the API)

{
  "path": "project.uatc",
  "formatVersion": { "major": 1, "minor": 0 },
  "fileCount": 12,
  "blockCount": 700,
  "originalBytes": 734003200,
  "archiveBytes": 412000000,
  "ratio": 1.78,
  "storedBlocks": 41,
  "compressedBlocks": 659,
  "transformUsage": [
    { "id": 0, "name": "None", "blocks": 300 },
    { "id": 1, "name": "Delta", "blocks": 120 },
    { "id": 2, "name": "XOR", "blocks": 30 },
    { "id": 3, "name": "Byte-shuffle", "blocks": 50 },
    { "id": 4, "name": "RLE", "blocks": 200 }
  ],
  "codecUsage": [
    { "id": 0, "name": "raw", "blocks": 41 },
    { "id": 1, "name": "Huffman", "blocks": 659 }
  ]
}

## 4. What `verify` reports

- status: ok or failed
- filesChecked, blocksChecked
- on failure: one error (see api-contract.md) with details such as the block index

## 5. To confirm with teammates

- Chakshu: can `info(path)` in the core facade return the fields in section 1
  from the Block Index and File Table alone? Please add any field you think is missing.
- Sai Rishi: transform names come from the registry. I will display exactly the
  names above unless you change them.
- Sampada: elapsedMs and throughputMBps in the job result come from your instrumentation.
