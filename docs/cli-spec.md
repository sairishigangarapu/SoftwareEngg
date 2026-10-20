# UATC CLI Spec (draft)

Owner: sub-commands belong to their module owners; Manasa K integrates
and owns `info` and `verify`. Source: SRS 3.2.1 and 4, SAD 4.3.3, 4.3.4, 4.5.

General form:  uatc <command> [arguments] [options]

## Commands

uatc compress <input> <output.uatc> [--workers N] [--block-size BYTES]
  input is a file or a directory. Writes the archive to output.uatc.
  Example: uatc compress ./dataset.bin ./dataset.uatc --workers 8
  Same input with --workers 1 must give a byte-identical archive (FR-10).

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
--verbose        extra detail

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
