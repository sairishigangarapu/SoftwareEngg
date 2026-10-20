# HTTP API Contract (draft)

Owner: Manasa K (FR-15). Source: SAD 4.3.1, 4.3.4, 3.9. Items marked
PROPOSED are not in the SAD yet and need the team's OK.

Base path /api. JSON, UTF-8. Server listens on 127.0.0.1 only.
PROPOSED: every request sends the session token in header X-UATC-Token.

## Endpoints

GET  /api/health                 -> 200 { "status": "ok", "version": "1.0" }
GET  /api/fs/list?path=          -> 200 { "path", "entries": [{ "name", "type", "size" }] }
POST /api/jobs/compress          -> 202 { "jobId" }
POST /api/jobs/extract           -> 202 { "jobId" }
POST /api/jobs/verify            -> 202 { "jobId" }
GET  /api/jobs/{id}              -> 200 { "state", "progress", "result" | "error" }
GET  /api/archives/info?path=    -> 200 info object (docs/inspection-data.md section 3)

## Requests

compress: { "source": "/home/user/project", "destination": "/home/user/project.uatc",
            "workers": 8, "blockSize": 1048576 }      // workers, blockSize optional
extract:  { "archive": "/home/user/project.uatc", "destination": "/home/user/restored" }
verify:   { "archive": "/home/user/project.uatc" }

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

extract (PROPOSED): result { "files": 12, "bytes": 734003200, "elapsedMs": 4100 }
verify  (PROPOSED): result { "status": "ok", "filesChecked": 12, "blocksChecked": 700 }

## Error

A failed job still returns HTTP 200 with state "failed":
{ "state": "failed",
  "error": { "code": "E_BLOCK_CHECKSUM",
             "message": "block 213 CRC32 mismatch",
             "userMessage": "This archive is corrupted and cannot be extracted safely.",
             "details": { "block": 213 } } }

Request-level errors: 400 E_INVALID_ARGUMENT, 403 E_PERMISSION, 404 E_NOT_FOUND.
Unknown job id: 404 E_NOT_FOUND.

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
