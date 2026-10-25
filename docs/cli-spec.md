# UATC CLI Spec (draft)

Owner: sub-commands belong to their module owners; Manasa K integrates
and owns `info` and `verify`. Source: SRS 3.2.1 and 4, SAD 4.3.3, 4.3.4, 4.5.

General form:  uatc <command> [arguments] [options]

## Commands

uatc stats <input> [--block-size BYTES]
  Analyses the file and reports what the engine will do to it, without
  writing anything: the size, the overall entropy, the per-block entropy
  and unique-byte count, whether each block is compressible, the transform
  that will be selected for it, and the predicted archive size and ratio.
  The prediction comes from the fast cost estimator, so it is an upper
  bound: the real archive can come out smaller.

uatc compress <input> <output.uatc> [--workers N] [--block-size BYTES]
  input is a file or a directory. Writes the archive to output.uatc.
  Example: uatc compress ./dataset.bin ./dataset.uatc --workers 8
  Same input with --workers 1 must give a byte-identical archive (FR-10).
  Reports per-block progress on stderr as each block is processed: the
  block number, the bytes in and out, the transform chosen, and the
  percentage complete.

uatc extract <archive.uatc> <output-dir>
  Restores files and folders under output-dir, byte for byte.
  Each block is checked (CRC32) before it is written. On a failure it
  stops, reports the error and removes partial output.

uatc info <archive.uatc>
  Prints the fields in docs/inspection-data.md section 1.
  Does not extract anything.

uatc verify <archive.uatc>
  Checks magic, version, structure and every checksum. Writes nothing to disk.
  Prints a success line, or exactly what failed (error code and block index).

uatc benchmark <sample-dir>
  Runs UATC, gzip/DEFLATE and Zstandard on the same samples and prints
  ratio, speed and resource use. Owner: Sampada.

uatc --help, uatc <command> --help

## Common options (SAD 4.5)

--workers N      worker threads (default: chosen by the core)
--block-size N   block size in bytes (default 1 MiB)
--quiet          only errors

Progress goes to stderr, results go to stdout, so output can be scripted.

## Exit codes (SAD 4.3.4)

0  success
1  E_INTERNAL
2  E_INVALID_ARGUMENT
3  E_NOT_FOUND
4  E_PERMISSION
5  E_IO / E_DISK_FULL
10 E_MALFORMED_ARCHIVE
11 E_UNSUPPORTED_VERSION
12 E_MISSING_BLOCK
13 E_BLOCK_CHECKSUM
14 E_ARCHIVE_CHECKSUM
15 E_PATH_TRAVERSAL

Errors are printed to stderr as:  error: <CODE>: <message>

## Example `info` output

Archive:      project.uatc (format 1.0)
Files:        12
Blocks:       700 (659 compressed, 41 stored as-is)
Original:     734003200 bytes
Archive:      412000000 bytes
Ratio:        1.78
Transforms:   None 300, Delta 120, XOR 30, Byte-shuffle 50, RLE 200
Codecs:       raw 41, Huffman 659

## Example `verify` output

OK: 12 files, 700 blocks verified.
or
error: E_BLOCK_CHECKSUM: block 213 failed its checksum.

## Example `stats` output

File:              ./dataset.bin
Size:              1000000 bytes
Block size:        262144 bytes
Blocks:            4
Entropy:           5.505 bits/byte
Compressible:      3 block(s)
Stored as-is:      1 block(s)
Predicted archive: 830442 bytes
Predicted ratio:   1.69

The prediction comes from the fast cost estimator, so it is
an upper bound: the real archive can come out smaller.

Per block:
  #   offset      size   entropy  unique  transform        predicted
   0          0     65536    4.981     256  ByteShuffle         32645
   1      65536     65536    5.465     256  ByteShuffle         49023
   2     131072     65536    5.465     256  ByteShuffle         49025
   3     196608     65536    8.000     256  stored             65536

## Example `compress` progress (stderr)

  block    1/4     262144 ->      76288 B  ByteShuffle  25.0%
  block    2/4     262144 ->      76288 B  ByteShuffle  50.0%
  block    3/4     262144 ->      24964 B  ByteShuffle  75.0%
  block    4/4     262144 ->     262144 B  stored      100.0%
