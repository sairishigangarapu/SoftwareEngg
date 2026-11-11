#pragma once

// HTTP API layer and job manager (FR-13/14/15, SAD 4.3.1).
//
// A minimal localhost-only HTTP server with the endpoints from
// docs/api-contract.md, and the in-memory job table from
// docs/progress-states.md. No external dependency: the requests are parsed
// by hand and answered with hand-built JSON, the same shapes the
// frontend's api() wrapper expects.
//
// Security posture (SAD 3.9, STP 5.1):
//   - Spoofing (TC-SEC-03): the server binds to 127.0.0.1 only, rejects a
//     Host header that is not a loopback name (DNS rebinding), rejects a
//     browser Origin that is not on the allow-list, and requires the
//     per-session token in the X-UATC-Token header on every /api call
//     except /api/health.
//   - Information disclosure (TC-API-04, TC-SEC-04): every path a request
//     carries is normalised and must resolve inside the allowed root, or
//     the request is refused with 403 E_PERMISSION.
//   - Denial of service: header and body sizes are capped, numbers are
//     range-checked, and a malformed request is a 400, never a crash.

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/compressor.hpp"
#include "core/error.hpp"
#include "core/progress.hpp"

namespace uatc::server {

// The states from docs/progress-states.md.
enum class JobState { Queued, Running, Completed, Failed };

const char* job_state_name(JobState state);

// One entry of a job's per-transform breakdown (SAD 4.3.1 example).
struct TransformCount {
    int id = 0;
    std::string name;
    std::uint64_t blocks = 0;
};

struct Job {
    std::string id;
    std::string operation;          // compress | extract | verify
    JobState state = JobState::Queued;
    double progress = 0.0;          // 0.0 .. 1.0
    // Failure, filled when state == Failed.
    core::ErrorCode error = core::ErrorCode::Internal;
    std::string error_message;      // technical detail, for logs and devs
    // The result fields, filled on completion.
    std::uint64_t originalBytes = 0;
    std::uint64_t archiveBytes = 0;
    double ratio = 0.0;
    std::uint64_t elapsedMs = 0;
    std::uint64_t blocks = 0;
    std::uint64_t storedBlocks = 0;
    std::vector<TransformCount> transformUsage;
};

// ------------------------------------------------------------- errors
//
// The SAD 4.3.4 table, shared by the job runner and the request handlers
// so the CLI, the API and the UI report one failure the same way.

// The plain-language message the UI shows (SAD 4.3.4, last column).
const char* user_message(core::ErrorCode code);

// The HTTP status for a request-level error (SAD 4.3.4, HTTP column).
int http_status_for(core::ErrorCode code);

// Maps any exception thrown by the core, the archive layer or the
// filesystem to its SAD error code. The archive layer still throws
// std::runtime_error for format problems, so those are classified by
// message; everything unrecognised there is E_MALFORMED_ARCHIVE.
core::ErrorCode classify_error(const std::exception& error);

// {"error":{"code":..., "message":..., "userMessage":...}}
std::string error_json(core::ErrorCode code, const std::string& message);

// ---------------------------------------------------------- path rule

// Resolves a path supplied by an API request against the allowed root and
// returns its canonical form. The path may be absolute or relative to the
// root, and the target need not exist yet (a destination). Symlinks in the
// existing part of the path are resolved before the check, so a link
// cannot be used to leave the root.
//
// Throws UatcError:
//   E_INVALID_ARGUMENT - empty, or contains a NUL byte
//   E_PERMISSION       - resolves outside the allowed root
std::filesystem::path resolve_in_root(const std::filesystem::path& root,
                                      const std::string& requested);

// ------------------------------------------------------------- jobs

// The job table plus the threads that run jobs. One instance per server.
// Every read and write of a Job goes through mutex_, so the HTTP thread
// never sees a half-updated job (TSan clean).
class JobManager {
 public:
    JobManager() = default;
    ~JobManager();
    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    // Submits a job and returns its id (j-0001, j-0002, ...).
    std::string submit_compress(const std::filesystem::path& source,
                                const std::filesystem::path& archive,
                                std::size_t block_size, std::size_t workers);
    std::string submit_extract(const std::filesystem::path& archive,
                               const std::filesystem::path& destination);
    std::string submit_verify(const std::filesystem::path& archive);

    // A consistent snapshot of a job, or nothing when the id is unknown.
    std::optional<Job> get(const std::string& id) const;

    void wait_until_empty();     // for tests and shutdown

 private:
    // Registers a new job and returns its id.
    std::string create(const std::string& operation);
    // Applies an update to a job under the lock.
    template <typename Fn>
    void update(const std::string& id, Fn&& fn);
    // Runs body on a new thread with the shared state and error handling:
    // Running on start, Completed on return, Failed with the classified
    // error code if body throws.
    template <typename Body>
    void launch(const std::string& id, Body body);

    mutable std::mutex mutex_;
    std::map<std::string, Job> jobs_;
    std::uint64_t next_id_ = 0;
    std::vector<std::thread> workers_;
};

// ------------------------------------------------------------- server

struct ServerOptions {
    std::uint16_t port = 8080;
    // Every path a request names must resolve inside this directory.
    std::filesystem::path allowed_root = std::filesystem::current_path();
    // The session token. Empty means "generate a random one".
    std::string token;
    // Browser origins allowed to call the API, besides the server's own
    // (http://127.0.0.1:<port> and http://localhost:<port>).
    std::vector<std::string> allowed_origins = {
        "http://localhost:8000", "http://127.0.0.1:8000"};
    // When set, GET / serves index.html, app.js and styles.css from this
    // directory, so the frontend can run from the server's own origin.
    std::filesystem::path web_root;
};

// A random 128-bit token as 32 lowercase hex characters.
std::string generate_token();

// Blocking server: listens on 127.0.0.1:port. serve_forever() runs the
// accept loop on the calling thread until stop() is called; each request is
// handled on it, one at a time - the API is a local single-user control
// plane, not a web service.
class ApiServer {
 public:
    explicit ApiServer(ServerOptions options);
    explicit ApiServer(std::uint16_t port);   // defaults for everything else
    ~ApiServer();

    void serve_forever();
    void stop();

    const std::string& token() const { return options_.token; }
    const std::filesystem::path& allowed_root() const {
        return options_.allowed_root;
    }
    JobManager& jobs() { return job_manager_; }

 private:
    void handle_client(int client_fd);

    ServerOptions options_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    JobManager job_manager_;
};

}  // namespace uatc::server
