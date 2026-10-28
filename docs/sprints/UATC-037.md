# UATC-037: Day 15 - Path traversal hardening

**Milestone**: Milestone 3 (Days 11-17) - Hardening and Parallelism
**Assignee**: Sai Rishi Gangarapu
**Priority**: P1
**Requirements**: FR-03, FR-12 (closes UATC-030)

- [x] safe_relative_path() in the core: one shared rule for the CLI, extraction and the future API layer
- [x] Rejects absolute paths, drive and UNC roots, any '..' segment, and paths whose normalised form is absolute or empty
- [x] Extraction refuses with E_PATH_TRAVERSAL before creating anything
- [x] tests/core/path_safety_test.cpp covers the rule (14 hand-crafted cases) and proves nothing is written outside the destination (TC-SEC-01)

**Acceptance**: every escape refused; no partial output.

**GitHub**: #50
