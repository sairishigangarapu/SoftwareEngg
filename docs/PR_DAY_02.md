## Day 2: inspection checklist, error categories, wireframes, progress states

Docs only. No code changes, no edits to Day 1 files.

### What is added
- `docs/inspection-checklist.md`: region-by-region list of what `info` shows and `verify` checks, with the order of checks
- `docs/error-categories.md`: the four archive error categories, full code catalogue, how CLI/API/UI report them
- `docs/wireframes.md`: Compress, Extract, Info, Verify, Progress, Result, Error screens
- `docs/progress-states.md`: queued / running / completed / failed, progress rules, polling

### Traceability
FR-11, FR-12, FR-13, FR-14, FR-15. SAD 3.5, 3.9, 4.1, 4.3.1, 4.3.4, 4.4, 4.5.
Tests: TC-INFO-01, TC-VER-01, TC-ERR-01..04, TC-INT-01/02, TC-API-01/03, TC-UI-01..05.

### Needs a decision (all marked PROPOSED in the docs)
1. Home gets four choices (adds Info and Verify). SAD 4.5 says two. @team
2. Which code for header-checksum failure, truncated file, footer damage. @Chakshu
3. STP TC-INT-02 says "corrupt archive-level checksum / footer"; these are two cases. @Chakshu
4. Unknown transform/codec ID: which code (decide Day 7). @Sai Rishi
5. Running progress capped at 0.99; `1.0` only with `completed`. @Sampada
6. Unknown job ID shows the E_INTERNAL text, not the "file or folder" text.

### Follow-ups (not in this PR)
- api-contract.md: add 422 for archive errors on direct calls (`/archives/info`), `progress` optional on `failed`
- app.js: replace `setInterval` polling with chained `setTimeout`, add elapsed time (Day 13/14)

### Checklist
- [x] Docs match SAD 4.3.4 messages and exit codes
- [x] Every PROPOSED item is labelled
- [ ] Reviewed by a teammate
