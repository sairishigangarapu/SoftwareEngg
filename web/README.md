# UATC web frontend

Owner: Manasa K (FR-13, FR-14, FR-15). Design: SAD 4.5,
`docs/wireframes.md`. API: `docs/api-contract.md`.

A browser interface for people who do not want to use the command line.
It is presentation only: every compression, extraction and check runs in
the C++ backend (`uatc serve`), reached over a local HTTP API.

| File | What it is |
|---|---|
| `index.html` | The five screens (Home, Select, Progress, Result, Error) and the file-browser dialog |
| `app.js` | Screen logic, the API client, the file browser, polling, and the demo-mode stand-in |
| `styles.css` | Black-on-white styling, visible focus, one column on narrow screens |

## Running it

### With real files (live mode)

```bash
make cli
```
```bash
build/uatc_cli serve --root ~/Documents
```

The server prints a line like

    open the app:   http://127.0.0.1:8080/?live#token=3f9c...

Open exactly that address. The part after `#token=` is the session key:
every API call must carry it (SAD 3.9), and it is never sent anywhere
except back to the server in a header. `--root` is the only folder the
page can see or write to; it defaults to the folder you start the server
in.

### Without a backend (demo mode)

Open `index.html` through any static server, without `?live`:

```bash
python3 -m http.server 8000 --directory web
```

Then open http://localhost:8000. A banner says demo mode is on, a small
pretend folder tree is shown in the browser, and every result is labelled
"Demo only". Add `?fail` (http://localhost:8000/?fail) to see the error
screen. Demo mode is for walkthroughs and UI tests; it reads and writes
nothing.

To use the port-8000 page against the real backend, start `uatc serve`
too and open `http://localhost:8000/?live#token=<token>`. The server
allows that origin by default.

## Screens (SAD 4.5)

| Screen | What it does | API call |
|---|---|---|
| Home | Four choices: Compress, Extract, Archive Info, Verify Archive | none |
| Select | Source and destination, each with **Browse…**; Advanced holds the worker count (compress only). The destination is suggested from the source | `GET /api/fs/list` while browsing |
| Progress | Status line, progress bar, percentage, elapsed time. Polls every 500 ms (ADR-07) | `POST /api/jobs/{op}`, then `GET /api/jobs/{id}` |
| Result | Compress: sizes, ratio, time, speed, stored blocks, method table. Extract: files and bytes restored. Verify: files and blocks checked. Info: archive details and its file list | job result, or `GET /api/archives/info` |
| Error | One plain-language sentence and **Try again** (keeps what you typed) / **Start over** | none |

Archive Info is a single request, so it goes straight to the Result screen.

## Rules this page follows

- **No raw codes.** Errors show the backend's `userMessage`, or the SAD
  4.3.4 sentence for the code. Never the code, a block number or a stack
  trace (TC-UI-04).
- **Never freezes.** Polling uses a `setTimeout` chain, so one slow reply
  cannot pile up requests; replies for a job the user has left are ignored.
  "Back to start" stops watching but the job keeps running in the backend
  (NFR-08, TC-UI-05).
- **Accessible.** Every input has a label; focus moves to each new
  screen's heading; errors use `role="alert"`, progress uses
  `role="status"`; the file browser is a native modal `<dialog>` (Esc
  closes it); every control works from the keyboard and focus is always
  visible; success and failure are shown with text and symbols, never by
  colour alone; there is a "Skip to content" link.
- **Safe output.** Anything that came from a file name or the server is
  inserted as text or through `escapeHtml()`, never as raw HTML.
- **Works from laptop to phone width.** One column below 34rem, no
  horizontal scrolling.

## Element IDs (stable for the Selenium tests)

| Area | IDs |
|---|---|
| Home | `btn-compress`, `btn-extract`, `btn-info`, `btn-verify` |
| Select | `select-title`, `source`, `btn-browse-source`, `destination`, `btn-browse-destination`, `workers`, `form-error`, `btn-start`, `btn-back` |
| Browser dialog | `browser`, `browser-path`, `browser-up`, `browser-list`, `browser-select`, `browser-cancel` |
| Progress | `progress-status`, `progress-bar`, `progress-text`, `progress-elapsed`, `btn-progress-home` |
| Result | `result-title`, `result-body`, `btn-again` |
| Error | `error-text`, `btn-error-retry`, `btn-error-back` |
| Mode | `mode-banner` |

Change an ID only together with the UI tests that use it.

## Test cases

TC-UI-01 (compress a folder), TC-UI-02 (extract), TC-UI-03 (progress and
results), TC-UI-04 (plain-language errors), TC-UI-05 (responsive during a
long job), TC-INFO-01 and TC-VER-01 through the UI. The automated Selenium
suite for these is the next item of work.
