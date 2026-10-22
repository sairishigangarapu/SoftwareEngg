# UATC-032: Make the transform cost estimate honest and more accurate

**Milestone**: Milestone 2 (Days 6-10) - Adaptive Engine  
**Assignee**: Sai Rishi Gangarapu (Team Lead)  
**Priority**: P1  
**Requirements**: FR-06, FR-08  
**GitHub**: #45

## Description

The `estimateCost` numbers are used to **rank** the candidate transforms, and for
that they work well. But they are also shown to the user in `uatc stats` as an
estimated size, and there they are badly wrong.

## Measured against the real archives

| File | Estimated payload | Actual payload | Error |
|------|-------------------|----------------|-------|
| zeros.bin (500 kB of zeros) | 2 bytes | 7 814 bytes | 4 000x too small |
| english.txt | 1 863 603 | 1 954 535 | 5% too small |
| mixed.bin | 830 442 | 581 284 | 43% too large |
| uint32_table.bin | 731 629 | 291 036 | 151% too large |

## Why it is wrong

1. **The baseline coder caps a repeat run at 128 bytes.** `estimate_coded_size`
   counts a run of three or more identical bytes as 2 bytes, so 262 144 zeros
   estimate to 2 bytes. The real cost is 2 048 runs of 2 bytes.
2. **The estimate does not notice that a transform can enlarge a block.** Delta
   turns a 262 144-byte block of English text into 264 186 bytes, and the
   estimate still reports a win.

## Tasks

- [x] Make the user-facing output honest: the estimate is described as a
      ranking heuristic, not a size prediction, and the field is renamed from
      "Predicted archive" to "Estimated payload"
- [ ] Account for the baseline coder's 128-byte repeat cap in
      `estimate_coded_size`
- [ ] Account for the transform's own expansion, so a block that Delta would
      enlarge is not reported as a win
- [ ] Consider pricing the *transformed* size rather than counting non-zero
      bytes in the original

## Why it matters

The estimate decides which transform wins, so a bad estimate does not corrupt
the archive, it only makes the engine pick a slightly worse transform. But it
is user-visible in `stats`, and a guide who compares "Estimated payload" with
the real archive size will spot the gap immediately.

## Acceptance Criteria

- The estimate is within a stated tolerance of the real payload for every
  corpus category, or the output says plainly that it is a ranking heuristic only
- The transform's own expansion is accounted for, so a block that Delta would
  enlarge is not reported as a win
- The repeat-run cap of the baseline coder is reflected in the size estimate
