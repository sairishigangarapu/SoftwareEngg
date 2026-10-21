# UATC-030: Reject path traversal when the archive is extracted to disk

**Sprint**: Sprint 8 (Hardening)  
**Assignee**: S Chakshu (archive layer) with Manasa K (API and CLI)  
**Priority**: P1  
**Requirements**: FR-03, FR-12  
**Test case**: TC-SEC-01 (Security validation, STP section 5.1)  
**GitHub**: #42

## Description

Make sure an archive can never write outside the chosen destination directory.

## Background

`CompressionManager::extract()` already rejects an absolute path or a path
containing `..` with `E_PATH_TRAVERSAL`. What is still missing is the
equivalent check for every path that reaches the HTTP API, and the archive
reader does not yet validate the recorded path itself.

The archive reader currently returns the file's bytes in memory and does not
write to disk, so there is no extraction-to-path code to attack yet. This must
land before the API and CLI gain real extraction.

## Tasks

- [ ] Apply the same normalisation and allowed-root check to every path that reaches the API layer: `/api/fs/list`, and the job source and destination paths
- [ ] Reject the archive with `E_PATH_TRAVERSAL` and write nothing at all, so there is no partial output
- [ ] Never follow a symlink out of the destination directory
- [ ] Add the test case: an archive containing `../../evil.txt` and one containing an absolute path are both rejected, and nothing is written outside the destination (TC-SEC-01)
- [ ] Add the API test: a path outside the allowed root is refused with 403 `E_PERMISSION` and no listing is returned (TC-API-04)

## Acceptance Criteria

- Absolute paths and `..` segments are rejected with `E_PATH_TRAVERSAL`
- Nothing is written outside the chosen destination directory
- The check is applied to the API paths as well as to the archive contents
