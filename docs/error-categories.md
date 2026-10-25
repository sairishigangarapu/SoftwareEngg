# Error Categories

Owner: Manasa K (FR-12, FR-15). Day 2.
Source: SAD 4.3.4 (codes, exit codes, UI messages), SAD 3.9, 4.4,
STP Appendix A (TC-ERR, TC-INT, TC-SEC).
Items marked PROPOSED are not in the SAD yet and need the team's OK.

## 1. Rule

Each failure has its own code. The core returns errors as values
(`Error{code, message, details}`), never as exceptions across modules
(SAD 4.4). The CLI maps the code to an exit code, the API maps it to JSON,
the UI shows only `userMessage`.

## 2. The four categories required for archives (Day 2 task)

| Category | Code | Typical cause | Details field | Detected at |
| --- | --- | --- | --- | --- |
| Malformed archive | E_MALFORMED_ARCHIVE | bad magic, not a UATC file, broken or truncated structure, impossible lengths or counts | `{ "reason": "bad magic" }` | checklist steps 1, 3, 4 |
| Missing block | E_MISSING_BLOCK | index points past the end of the file, payload cut off | `{ "block": N }` | checklist step 5 |
| Checksum failure | E_BLOCK_CHECKSUM | block CRC32 mismatch | `{ "block": N }` | checklist step 6 |
| Checksum failure | E_ARCHIVE_CHECKSUM | file or archive SHA-256 mismatch | `{ "file": "path" }` or `{}` | checklist step 7 |
| Unsupported version | E_UNSUPPORTED_VERSION | major version newer than the reader | `{ "major": N, "minor": M }` | checklist step 2 |

`details.reason` is PROPOSED. It helps tests tell causes apart and goes to
logs and CLI `--verbose` only. The UI never shows it.

## 3. Full catalogue (SAD 4.3.4)

| Code | CLI exit | HTTP (direct call) | UI message (shown as-is) |
| --- | --- | --- | --- |
| E_INTERNAL | 1 | 500 | Something went wrong inside UATC. Please try again. |
| E_INVALID_ARGUMENT | 2 | 400 | Something in the request is not valid. Please check the file or folder you selected. |
| E_NOT_FOUND | 3 | 404 | We could not find that file or folder. |
| E_PERMISSION | 4 | 403 | UATC is not allowed to open or write that location. |
| E_IO / E_DISK_FULL | 5 | 500 | The operation stopped because the disk could not be read or written (it may be full). |
| E_MALFORMED_ARCHIVE | 10 | 422 | This file is not a valid UATC archive or is damaged. |
| E_UNSUPPORTED_VERSION | 11 | 422 | This archive was made by a newer version of UATC. |
| E_MISSING_BLOCK | 12 | 422 | Part of this archive is missing. |
| E_BLOCK_CHECKSUM | 13 | 422 | This archive is corrupted and cannot be extracted safely. |
| E_ARCHIVE_CHECKSUM | 14 | 422 | This archive failed its integrity check. |
| E_PATH_TRAVERSAL | 15 | 422 | The archive contains an unsafe file path and was stopped. |

These strings are copied from the SAD and match `web/app.js` (`MESSAGES`)
and `docs/api-contract.md`. If the SAD wording changes, change all three.

## 4. How each surface reports an error

| Surface | Format |
| --- | --- |
| CLI | stderr: `error: <CODE>: <message>`, then exit code from the table. Block index in the message when known. |
| API, job (compress / extract / verify) | HTTP 200, `state: "failed"`, `error: { code, message, userMessage, details }` |
| API, request rejected before a job exists | HTTP 400 / 403 / 404 with `{ "error": {...} }` |
| API, direct call (`/archives/info`, `/fs/list`) | HTTP status from the table above (422 for archive errors) with `{ "error": {...} }` |
| UI | Error screen: heading "Something went wrong", `userMessage` in a `role="alert"` paragraph. No code, no stack trace (STP TC-UI-04). |
| Logs | job ID, operation, error code, details. Never file contents (SAD 4.4). |

Cleanup on any failure: temp and partial output of the job is removed
(SAD 4.4); a block is written only after its CRC32 matches.

## 5. Decisions and open questions (PROPOSED)

Written so that each corruption class from Day 15 gives exactly one code.

| # | Case | Proposed code | Why / who to confirm |
| --- | --- | --- | --- |
| 1 | Header checksum mismatch | E_MALFORMED_ARCHIVE | The structure cannot be trusted, so it is a structural problem rather than an integrity one. Confirm with Chakshu. |
| 2 | Truncated inside File Table or Block Index | E_MALFORMED_ARCHIVE | Index cannot be read. |
| 3 | Truncated inside payloads (index intact) | E_MISSING_BLOCK | `offset + stored length` goes past end of file. |
| 4 | Only the footer is missing or damaged | E_MALFORMED_ARCHIVE | Footer holds header offset and magic. |
| 5 | Stored archive SHA-256 differs | E_ARCHIVE_CHECKSUM | TC-INT-02 says "corrupt archive-level checksum / footer". These are two different cases (rows 4 and 5), so the STP wording should be split or the test should corrupt the SHA-256 field. |
| 6 | Absurd block length or block count (TC-SEC-02) | E_MALFORMED_ARCHIVE | Rejected before any allocation. |
| 7 | Unknown transform or codec ID in a supported major version | open | SAD has no code for it. E_UNSUPPORTED_VERSION fits the UI message ("newer version") best. Decide with Sai Rishi on Day 7, when unsupported transform IDs are documented. |
| 8 | Unknown job ID | HTTP 404 E_NOT_FOUND | The UI message says "file or folder", which is wrong for a job. UI should treat it as E_INTERNAL text. See progress-states.md. |
| 9 | Order of checks | first failure wins, order in `inspection-checklist.md` section 3 | Needed so verify and extract report the same code for the same archive. |

## 6. Test mapping

| Code | Test cases |
| --- | --- |
| E_NOT_FOUND, E_PERMISSION | TC-FS-03, TC-API-03, TC-API-04 |
| E_INVALID_ARGUMENT | TC-API-02, TC-SEC-04 |
| E_IO / E_DISK_FULL | TC-API-03 |
| E_MALFORMED_ARCHIVE | TC-ERR-01, TC-ERR-03, TC-SEC-02 |
| E_UNSUPPORTED_VERSION | TC-ERR-02 |
| E_MISSING_BLOCK | TC-ERR-01 |
| E_BLOCK_CHECKSUM | TC-INT-01 |
| E_ARCHIVE_CHECKSUM | TC-INT-02 |
| E_PATH_TRAVERSAL | TC-SEC-01 |
| All distinct | TC-ERR-04 |
| UI shows plain text only | TC-UI-04 |
