# UI Wireframes

Owner: Manasa K (FR-13, FR-14). Day 2.
Source: SAD 4.5 (UX), 4.3.1 (API), api-contract.md, inspection-data.md,
progress-states.md, error-categories.md.
Low-fidelity, text only. They show layout, labels and which API call each
screen uses. Visual style stays as in `web/styles.css` (black on white,
visible focus).

## 0. Screen map

```
Home -> Compress / Extract / Info / Verify (select) -> Progress -> Result
                                                  \-> Error (any step)
Info has no Progress: it is one synchronous call.
```

| Screen | In Day 1 shell? | API call |
| --- | --- | --- |
| Home | yes (2 buttons) | none |
| Select: Compress / Extract | yes (text inputs) | `GET /api/fs/list` for Browse (Day 3), then `POST /api/jobs/compress` or `/extract` |
| Select: Info / Verify | new | `GET /api/archives/info` / `POST /api/jobs/verify` |
| Progress | yes | `GET /api/jobs/{id}` every 500 ms |
| Result: compress, extract, verify, info | compress only | result from job, or from `/archives/info` |
| Error | yes | none (shows `userMessage`) |

**Change needing the team's OK (PROPOSED):** SAD 4.5 lists four screens and a
Home with two choices. The Execution Plan (Day 2, Day 14) asks for Info and
Verify screens, and the API already has endpoints for them. So Home gets four
choices, and Info/Verify reuse the same Select, Progress, Result and Error
screens. No new API is needed.

## 1. Home

```
+----------------------------------------------------------+
| UATC                                                     |
| Compress and extract files without losing a single byte. |
+----------------------------------------------------------+
| What do you want to do?                                  |
|                                                          |
|  [ Compress ]  [ Extract ]  [ Info ]  [ Verify ]         |
|                                                          |
|  Info:   look inside an archive without extracting it    |
|  Verify: check an archive is not damaged                 |
+----------------------------------------------------------+
```

## 2. Compress (Select)

```
+----------------------------------------------------------+
| Compress                                                 |
|                                                          |
| Source (file or folder)                                  |
| [ /home/user/project                      ] [ Browse ]   |
|                                                          |
| Destination archive                                      |
| [ /home/user/project.uatc                 ] [ Browse ]   |
|                                                          |
| > Advanced                                               |
|     Worker threads  [ 1 ]                                |
|                                                          |
| [ Start ]  [ Back ]                                      |
+----------------------------------------------------------+
```

Request: `{ source, destination, workers }` (`workers` optional).
Start is blocked, with a text message, if a path is empty.

## 3. Extract (Select)

```
+----------------------------------------------------------+
| Extract                                                  |
|                                                          |
| Archive to extract                                       |
| [ /home/user/project.uatc                 ] [ Browse ]   |
|                                                          |
| Extract into folder                                      |
| [ /home/user/restored                     ] [ Browse ]   |
|                                                          |
| [ Start ]  [ Back ]                                      |
+----------------------------------------------------------+
```

Request: `{ archive, destination }`. No Advanced section.

## 4. Info (Select, then Result)

```
+----------------------------------------------------------+
| Info                                                     |
|                                                          |
| Archive                                                  |
| [ /home/user/project.uatc                 ] [ Browse ]   |
|                                                          |
| [ Show info ]  [ Back ]                                  |
+----------------------------------------------------------+
```

Result (`GET /api/archives/info?path=`), nothing is extracted:

```
+----------------------------------------------------------+
| Archive information                                      |
|                                                          |
|  Archive file        project.uatc (format 1.0)           |
|  Files               12                                  |
|  Blocks              700                                 |
|  Original size       700.0 MB                            |
|  Archive size        392.9 MB                            |
|  Ratio               1.78                                |
|  Blocks stored as-is 41 of 700                           |
|                                                          |
|  Method used            Blocks                           |
|  None                   300                              |
|  Delta                  120                              |
|  XOR                     30                              |
|  Byte-shuffle            50                              |
|  RLE                    200                              |
|                                                          |
|  [ Start over ]                                          |
+----------------------------------------------------------+
```

No Time or Speed rows: they are not stored in the archive
(`inspection-data.md` section 2). The "stored as-is" explanation text
is Day 9 work.

## 5. Verify (Select)

```
+----------------------------------------------------------+
| Verify                                                   |
|                                                          |
| Archive to check                                         |
| [ /home/user/project.uatc                 ] [ Browse ]   |
|                                                          |
| Nothing is extracted or written.                         |
|                                                          |
| [ Check archive ]  [ Back ]                              |
+----------------------------------------------------------+
```

Request: `POST /api/jobs/verify { archive }`, then Progress.

## 6. Progress (shared by Compress, Extract, Verify)

```
+----------------------------------------------------------+
| Working                                                  |
|                                                          |
| Compressing...                (Extracting / Checking)    |
| [##########------------------]                           |
| 42%                                                      |
| Elapsed: 00:03                                           |
+----------------------------------------------------------+
```

`queued` shows "Waiting to start...". The percentage is text as well as bar
(not colour alone). `role="status"` so screen readers hear updates.
Day 1 has no elapsed-time line; add it when progress is wired up (Day 13/14).
No Cancel button in v1.

## 7. Result (Compress)

```
+----------------------------------------------------------+
| Done                                                     |
|                                                          |
|  Original size   700.0 MB                                |
|  Archive size    392.9 MB                                |
|  Ratio           1.78                                    |
|  Time            5.2 s                                   |
|  Speed           134.5 MB/s                              |
|  Blocks stored as-is  41 of 700                          |
|                                                          |
|  Method used            Blocks                           |
|  None 300 | Delta 120 | XOR 30 | Byte-shuffle 50 | RLE 200|
|                                                          |
|  [ Start over ]                                          |
+----------------------------------------------------------+
```

This is what `showResult()` in Day 1 `app.js` already renders.

## 8. Result (Extract) and Result (Verify)

Both result shapes are PROPOSED in `api-contract.md`.

```
Extract                                  Verify
+----------------------------+           +----------------------------+
| Done                       |           | Archive is OK              |
|                            |           |                            |
| Files restored   12        |           | Files checked    12        |
| Total size       700.0 MB  |           | Blocks checked   700       |
| Time             4.1 s     |           |                            |
|                            |           | Nothing was written.       |
| [ Start over ]             |           | [ Start over ]             |
+----------------------------+           +----------------------------+
```

## 9. Error (shared)

```
+----------------------------------------------------------+
| Something went wrong                                     |
|                                                          |
| This archive is corrupted and cannot be extracted safely.|
|                                                          |
| [ Start over ]                                           |
+----------------------------------------------------------+
```

Text is `error.userMessage` only (`error-categories.md` section 3). No code,
no block number, no stack trace (TC-UI-04). Shown for failed jobs, rejected
requests and failed `info` calls.

## 10. Rules for every screen

- One `h2` per screen; focus moves to it when the screen appears
  (already done by `show()` in `app.js`).
- Every input has a visible `label`.
- Every button is reachable and usable with the keyboard, focus is visible.
- Status and errors are text, never colour alone.
- Layout works at laptop and tablet width (single column, max width 40 rem).
- Element IDs stay stable for Selenium (STP risk table): `btn-compress`,
  `btn-extract`, `btn-start`, `btn-back`, `progress-bar`, `progress-text`,
  `result-body`, `error-text`. PROPOSED new IDs: `btn-info`, `btn-verify`,
  `btn-browse-source`, `btn-browse-destination`.

## 11. Test mapping

| Screen | Test cases |
| --- | --- |
| Home, Select (Compress) | TC-UI-01 |
| Select (Extract) | TC-UI-02 |
| Progress, Result | TC-UI-03 |
| Error | TC-UI-04 |
| Any screen while a job runs | TC-UI-05 |
| Info result | TC-INFO-01 |
| Verify result | TC-VER-01 |
