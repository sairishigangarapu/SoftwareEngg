# UATC-038: Day 16 - uatc benchmark and an exact cost estimator

**Milestone**: Milestone 3 (Days 11-17) - Hardening and Parallelism
**Assignee**: Sai Rishi Gangarapu
**Priority**: P1
**Requirements**: NFR-02, NFR-07 (closes UATC-032)

**The estimate** said 2 bytes for 500 kB of zeros and 1 863 603 for a payload of 1 954 535. It is now a byte-exact model of the baseline coder, verified equal to the real coder's output on 100 randomised trials:

- [x] estimate_coded_size mirrors the encoder's walk exactly: runs of three or more identical bytes become 2-byte repeats, everything else accumulates into literal runs of up to 128 bytes with one control byte each
- [x] Delta and XOR price their output streams on the fly; ByteShuffle prices a materialised shuffle order in a reusable per-thread scratch, so determinism holds on the parallel path
- [x] NoneTransform prices the input exactly, which makes the store-versus-compress decision honest: text is now correctly stored raw
- [x] One real bug fell out: stored blocks were tagged CodecId::Baseline while holding raw bytes, so a reader met a payload that was never encoded. Stored blocks are None/None end to end

**The benchmark**:
- [x] uatc benchmark <dir> [--gzip] [--workers N] [--out FILE]: ratio, time and MB/s per file, plus the gzip ratio on the same file when asked (NFR-02: report, do not claim)
- [x] Every record includes a round-trip check, so a broken compressor cannot produce a passing benchmark
- [x] JSONL output for plotting or diffing between runs

**GitHub**: #51
