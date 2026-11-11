# UATC - 15-minute Final Demo Script

**Owner**: Manasa K (demo workflow, synopsis section 15)
**Rubric (course guidelines)**: working demo and 15-minute presentation;
show the use cases, what makes UATC different, and every SDLC stage with
its output. Each member presents their own part and must be able to explain
their code.

## Before the session (10 minutes, once)

Run these on the demo laptop and leave the terminals open.

```bash
make all && make test
```
```bash
make cli
```
Prepare a demo folder with a little of everything:

```bash
mkdir -p ~/uatc-demo/project/src && cd ~/uatc-demo
```
```bash
seq 1 200000 | awk '{print $1","$1*3%101","$1%7}' > project/data.csv
```
```bash
cp -r /path/to/SoftwareEngg/docs project/docs && cp /path/to/SoftwareEngg/src/core/*.cpp project/src/
```
```bash
head -c 2000000 /dev/urandom > project/photo_like.bin
```
```bash
seq 1 1500000 | awk '{print $1",sensor-"$1%97","($1*37)%1000}' > big.csv
```

Start the server (terminal 1) and keep the printed address:

```bash
/path/to/SoftwareEngg/build/uatc_cli serve --root ~/uatc-demo
```

Open the printed `http://127.0.0.1:8080/?live#token=...` in the browser and
check the home screen appears. Open a second tab at the same address with
`?live` replaced by nothing (demo mode) as a fallback.

Fallback if anything breaks: demo mode (no backend) shows every screen;
`scripts/system_validation.sh` output and `docs/validation-report.md` show
the real results.

## Running order (15:00)

| Time | Part | Speaker | Shows |
|---|---|---|---|
| 0:00-1:30 | Problem and idea | Sai Rishi | SRS 1.2: one fixed pipeline vs per-block choice; store what does not shrink; parallel blocks |
| 1:30-3:00 | SDLC: requirements and design | Sai Rishi | SRS (FR-01..15), SAD (layered architecture, ADRs, archive format), STP; sprint plan and issues on GitHub |
| 3:00-6:00 | Use case UC-1: web UI | Manasa | Section A |
| 6:00-8:00 | Engine and differentiation | Sai Rishi | Section B |
| 8:00-9:30 | Archive format and integrity | Chakshu | Section C |
| 9:30-11:00 | Parallelism and performance | Sampada | Section D |
| 11:00-13:30 | Security and quality | Manasa | Section E |
| 13:30-15:00 | Validation, maintenance, wrap-up | Manasa | Section F |

## A. Web UI, UC-1 (Manasa, 3 min)

1. Home screen: "four things a non-technical user can do, no command line".
2. **Compress** -> **Browse...** -> open `project` -> **Use this folder**.
   Point out: the destination is suggested; the folder browser only shows
   the allowed folder (security, section E).
3. **Start**. Progress screen: status, bar, percentage, elapsed time.
   "The page polls every 500 ms; it never freezes, and Back to start leaves
   the job running."
4. Result: original size, archive size, ratio, time, speed, blocks stored
   as-is, method used per part. Read the "stored as-is" sentence: the
   random file was kept unchanged on purpose.
5. **Start over** -> **Archive Info** -> Browse to `project.uatc`: file list,
   methods, "Nothing was extracted".
6. **Extract** into `restored`, then in a terminal:
   ```bash
   diff -r ~/uatc-demo/project ~/uatc-demo/restored && echo IDENTICAL
   ```
   Say: "byte-for-byte, FR-03".

Code to have open if asked: `web/app.js` (`poll`, `showError`),
`src/server/api_server.cpp` (`resolve_in_root`, `handle_client`).

## B. Engine and what is different (Sai Rishi, 2 min)

```bash
build/uatc_cli stats ~/uatc-demo/project/data.csv --block-size 262144
```
```bash
build/uatc_cli compress ~/uatc-demo/project/data.csv /tmp/d.uatc --block-size 262144
```

Show the per-block decisions during compression: different blocks of one
file get different transforms, and a block that would not shrink is
stored. Explain cost-based selection (`T* = argmin metadata + encoded size`)
and that the cost model is exact for the coder.

Honest comparison (validation report 4.2): UATC wins on structured numeric
data (int32 ramp 3.53 vs gzip 2.88, zstd 1.46) and is behind on text,
because it has no LZ-style phrase matching; that is future work.

## C. Archive format and integrity (Chakshu, 1.5 min)

```bash
build/uatc_cli info ~/uatc-demo/project.uatc
```
```bash
build/uatc_cli verify ~/uatc-demo/project.uatc; echo "exit $?"
```

Corrupt one byte and verify again:

```bash
cp ~/uatc-demo/project.uatc /tmp/bad.uatc && python3 -c "import sys;p=sys.argv[1];b=bytearray(open(p,'rb').read());b[len(b)//2]^=0xFF;open(p,'wb').write(b)" /tmp/bad.uatc
```
```bash
build/uatc_cli verify /tmp/bad.uatc; echo "exit $?"
```

Say: CRC32 per block, SHA-256 per file and archive; each failure kind has
its own code and exit status (SAD 4.3.4). Show the same damaged archive in
the web UI -> plain-language error, no code.

## D. Parallelism (Sampada, 1.5 min)

```bash
for w in 1 2 4; do echo "workers $w"; time build/uatc_cli compress ~/uatc-demo/big.csv /tmp/w$w.uatc --workers $w --quiet; done
```
```bash
sha256sum /tmp/w1.uatc /tmp/w2.uatc /tmp/w4.uatc
```

Say: near-linear speed-up to the core count (validation report 4.1), and
the archive is identical for any worker count (ADR-05, FR-10); ThreadSanitizer
clean.

## E. Security and quality (Manasa, 2.5 min)

1. Path traversal: run the TC-SEC-01 section of the validation script, or
   show `tests/core/archive_service_test.cpp` "valid-checksum archive with
   ../ path": refused with E_PATH_TRAVERSAL, nothing written outside.
2. API protection, live:
   ```bash
   curl -s http://127.0.0.1:8080/api/fs/list
   ```
   -> 403 "Missing or wrong X-UATC-Token header" (TC-SEC-03).
3. Quality gates: the PR page on GitHub with the SonarCloud gate, the
   coverage numbers (core 91.0% lines, 81.1% branches), and the Selenium
   suite running:
   ```bash
   make ui-test
   ```

## F. Validation and maintenance (Manasa, 1.5 min)

- `docs/rtm.md`: every requirement traced to design, code and tests.
- `docs/validation-report.md`: 49 STP cases, 38 pass, 8 partial, 3 not
  run; the defects we found and fixed (D-01, D-05, D-06) and the two still
  open (D-03, D-04). "We report what is not done as clearly as what is."
- `docs/maintenance-plan.md`: ownership, CI gates, archive-compatibility
  rule (IDs never reused), backlog.
- Close: "a correct, measured, adaptive compressor, with every SDLC stage
  documented and tested."

## Questions to be ready for

| Question | Who | Short answer |
|---|---|---|
| Why per-block and not per-file? | Sai Rishi | Mixed files; each block picks its own representation; blocks decode independently, which also enables parallelism |
| How do you know extraction is lossless? | Chakshu | CRC32 per block checked before writing, SHA-256 per file and archive; 18-file corpus + 203-file tree verified with SHA-256 |
| What if two workers finish out of order? | Sampada | Results land in a pre-sized vector by block index; the writer emits in order; 1 vs 8 workers byte-identical |
| How is the web UI protected? | Manasa | Loopback only, per-session token, Origin and Host checks, allowed-folder path rule, strict input parsing |
| Why is text compression lower than gzip? | Sai Rishi | No LZ match finding yet; stated in the SRS as not a goal; listed as future work |
| What is not finished? | Manasa | D-03, D-04, fuzzing, dummy-transform test, second-OS run (validation report section 6) |
