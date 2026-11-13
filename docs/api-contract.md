# HTTP API Contract

Owner: Manasa K (FR-15). Source: SAD 4.3.1, 4.3.4, 3.9. Implemented in
`src/server/api_server.cpp`, tested by `tests/server/api_test.cpp`.

Base path /api. JSON, UTF-8. Server listens on 127.0.0.1 only.

## Starting the server

    uatc serve [--port 8080] [--root DIR] [--web DIR] [--allow-origin URL]

- `--root`: every path in a request must resolve inside this folder
  (default: the current folder).
- `--web`: folder with index.html, app.js and styles.css; the server serves
  them at `/` so the page and the API share one origin (default: `web`).
- `--allow-origin`: an extra browser origin allowed to call the API. The
  server's own origin and `http://localhost:8000` / `http://127.0.0.1:8000`
  (the `python3 -m http.server 8000 --directory web` dev setup) are always
  allowed.

The server prints a random 128-bit session token and the URL to open:
`http://127.0.0.1:8080/?live#token=<token>`. The token is in the URL
fragment, which the browser never sends to a server.

## Security rules (SAD 3.9, STP 5.1)

| Rule | Behaviour | Test case |
|---|---|---|
| Loopback only | Socket bound to 127.0.0.1; a `Host` header that is not `127.0.0.1` or `localhost` (DNS rebinding) gets 403 | TC-SEC-03 |
| Origin check | A request with an `Origin` not on the allow-list gets 403. Allowed origins are echoed in `Access-Control-Allow-Origin`; never `*` | TC-SEC-03 |
| Session token | Every `/api/*` call except `/api/health` must send `X-UATC-Token: <token>`; missing or wrong token gets 403 E_PERMISSION. Compared in constant time | TC-SEC-03 |
| Allowed root | Every path (query or body) is percent-decoded once, resolved against the root, symlinks followed, `..` normalised; anything outside the root gets 403 E_PERMISSION | TC-API-04, TC-SEC-04 |
| Input validation | Bodies must be flat JSON objects (no nesting, no duplicate keys, no trailing text). `workers` must be a whole number 1-256, `blockSize` 1024-67108864. Bad percent-encoding or a NUL byte in a path gets 400 | TC-API-02, TC-SEC-04 |
| Size limits | Headers over 16 KiB get 431, bodies over 64 KiB get 413 (uploads: 64 MiB), checked on `Content-Length` before the body is read; an idle client is dropped after 5 s | TC-SEC-04 |
| No overwrite | An upload never replaces an existing file: a taken name becomes `name (1).ext`, `name (2).ext`, ...; created atomically (`O_EXCL`) | D-13 |
| Bounded memory | Downloads are streamed in 64 KiB chunks; server memory does not grow with the file size (300 MB file: 3 MB peak) | D-14 |
| Token handling | Printed once at startup; never written into a response or an error message | SAD 3.9 |

## Endpoints

GET  /api/health                 -> 200 { "status": "ok", "version": "1.0" }
GET  /api/fs/list?path=          -> 200 { "path", "root", "entries": [{ "name", "type", "size" }] }
                                    (no path: lists the allowed root; entries sorted by name)
POST /api/jobs/compress          -> 202 { "jobId" }
POST /api/jobs/extract           -> 202 { "jobId" }
POST /api/jobs/verify            -> 202 { "jobId" }
GET  /api/jobs/{id}              -> 200 { "state", "progress", "result" | "error" }
GET  /api/archives/info?path=    -> 200 info object (docs/inspection-data.md section 3):
                                    path, formatVersion{major,minor}, fileCount, blockCount,
                                    originalBytes, archiveBytes, ratio, storedBlocks,
                                    compressedBlocks, transformUsage[{id,name,blocks}],
                                    codecUsage[{id,name,blocks}]
POST /api/fs/upload?name=&dir=  -> 200 { "path", "name", "renamed", "size" }
GET  /api/fs/download?path=      -> 200 the file's bytes (application/octet-stream)

## Requests

compress: { "source": "/home/user/project", "destination": "/home/user/project.uatc",
            "workers": 8, "blockSize": 1048576 }      // workers, blockSize optional
extract:  { "archive": "/home/user/project.uatc", "destination": "/home/user/restored" }
verify:   { "archive": "/home/user/project.uatc" }

## Upload and download

**Upload** sends a file from the browser into the allowed root, so the user
can compress files that are not already there.

- Request: `POST /api/fs/upload?name=<file name>&dir=<folder>`, body = the
  raw file bytes (`Content-Type: application/octet-stream`), at most 64 MiB.
- `name` is reduced to its last component (`../../a.txt` -> `a.txt`);
  empty, `.` and `..` are refused with 400.
- `dir` is optional and must resolve inside the allowed root (403
  otherwise). Without it, the file goes to an `uploads` folder inside the
  root.
- If the name is taken, the file is saved as `name (1).ext` and so on;
  `renamed` is then `true` and `name` is the name actually used. The UI
  tells the user ("A file called ... was already there, so this one was
  saved as ...").
- A write that fails part-way removes the partial file.

**Download** returns a file from the allowed root, for example the archive
just created.

- Request: `GET /api/fs/download?path=<file>`; 403 outside the root, 400 for
  a folder, 404 if missing.
- Response: `Content-Length`, `Content-Disposition: attachment;
  filename="<name>"` (quotes, backslashes and line breaks in the name become
  `_`), then the bytes, streamed.
- The browser fetches it with the token header and saves the blob, so the
  token never appears in a URL.

Tests: `tests/server/api_test.cpp`, section "upload / download".

## Progress

GET /api/jobs/j-0007 while running:
{ "state": "running", "progress": 0.42 }

state is one of: queued, running, completed, failed.
progress is 0.0 to 1.0, based on completed blocks. The UI polls about every 500 ms (ADR-07).

## Result (state = completed)

compress:
{ "state": "completed", "progress": 1.0,
  "result": { "originalBytes": 734003200, "archiveBytes": 412000000, "ratio": 1.78,
              "elapsedMs": 5210, "throughputMBps": 134.5, "blocks": 700, "stored": 41,
              "transformUsage": [{ "id": 1, "name": "Delta", "blocks": 120 }] } }

extract and verify use the same result object; `originalBytes` is the
number of bytes restored (extract) or recorded (verify), and verify adds
`"valid": true`. A verify that finds damage is a failed job with the
matching error code (E_BLOCK_CHECKSUM, E_ARCHIVE_CHECKSUM, ...).

## Error

A failed job still returns HTTP 200 with state "failed":
{ "state": "failed",
  "error": { "code": "E_BLOCK_CHECKSUM",
             "message": "block 213 CRC32 mismatch",
             "userMessage": "This archive is corrupted and cannot be extracted safely.",
             "details": { "block": 213 } } }

Request-level errors use the same error object with the SAD 4.3.4 HTTP
status: 400 E_INVALID_ARGUMENT, 403 E_PERMISSION, 404 E_NOT_FOUND,
422 for archive problems (E_MALFORMED_ARCHIVE, E_UNSUPPORTED_VERSION,
E_MISSING_BLOCK, E_BLOCK_CHECKSUM, E_ARCHIVE_CHECKSUM, E_PATH_TRAVERSAL),
500 E_IO / E_INTERNAL. Unknown job id: 404 E_NOT_FOUND.

Every error object has `code`, `message` (technical detail) and
`userMessage` (the plain-language text from the table below).

A job whose destination cannot be written fails with E_PERMISSION (no
write permission), E_IO (the parent is not a folder) or E_NOT_FOUND (the
destination folder does not exist) - TC-API-03.

The UI shows userMessage only. It never shows the raw code or a stack trace.

## Error codes and UI messages (SAD 4.3.4)

E_INVALID_ARGUMENT    Something in the request is not valid. Please check the file or folder you selected.
E_NOT_FOUND           We could not find that file or folder.
E_PERMISSION          UATC is not allowed to open or write that location.
E_IO / E_DISK_FULL    The operation stopped because the disk could not be read or written (it may be full).
E_MALFORMED_ARCHIVE   This file is not a valid UATC archive or is damaged.
E_UNSUPPORTED_VERSION This archive was made by a newer version of UATC.
E_MISSING_BLOCK       Part of this archive is missing.
E_BLOCK_CHECKSUM      This archive is corrupted and cannot be extracted safely.
E_ARCHIVE_CHECKSUM    This archive failed its integrity check.
E_PATH_TRAVERSAL      The archive contains an unsafe file path and was stopped.
E_INTERNAL            Something went wrong inside UATC. Please try again.
