#pragma once

// DAY 15 - Path safety (FR-03, FR-12, TC-SEC-01).
//
// One shared rule for every path an archive or an API request hands us:
// it must resolve inside the destination, and only there. The check lives
// in the core so the CLI, the extraction path and the HTTP API layer all
// get the same answer.

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"

namespace uatc::core {

// Validates that a path recorded in an archive, or supplied as a relative
// target, is safe to place under a destination directory:
//   - it is not absolute
//   - it has no '..' segment
//   - normalising it never escapes the root
// Returns the normalised path. Throws UatcError with E_PATH_TRAVERSAL if
// the path is unsafe.
std::filesystem::path safe_relative_path(const std::string& recorded_path);

// True when the same rule is satisfied, for callers that want to test
// without an exception.
bool is_safe_relative_path(const std::string& recorded_path);

}  // namespace uatc::core
