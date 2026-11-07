#pragma once

// DAY 19 - HTTP API layer and job manager (FR-13/14/15, SAD 4.3.1).
//
// A minimal localhost-only HTTP server with the endpoints from
// docs/api-contract.md, and the in-memory job table from
// docs/progress-states.md. No external dependency: the requests are parsed
// by hand and answered with hand-built JSON, the same shapes the
// frontend's api() wrapper already expects.
//
// Security posture (SAD 3.9): the server binds to 127.0.0.1 only, and
// every path a request carries goes through safe_relative_path() before
// it reaches the core (TC-API-04, TC-SEC-04).

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/compressor.hpp"
#include "core/progress.hpp"

namespace uatc::server {

// The states from docs/progress-states.md.
enum class JobState { Queued, Running, Completed, Failed };

const char* job_state_name(JobState state);

struct Job {
    std::string id;
    std::string operation;          // compress | extract | verify
    JobState state = JobState::Queued;
    double progress = 0.0;          // 0.0 .. 1.0
    std::string error_code;         // E_* on failure, empty otherwise
    std::string user_message;       // plain-language, for the UI
    // The result fields, filled on completion.
    std::uint64_t originalBytes = 0;
    std::uint64_t archiveBytes = 0;
    double ratio = 0.0;
    std::uint64_t elapsedMs = 0;
    std::vector<std::pair<std::string, std::uint64_t>> transformUsage;
};

// The job table plus the worker that runs jobs. One instance per server.
class JobManager {
 public:
    // Submits a job and returns its id (j-0001, j-0002, ...).
    std::string submit_compress(const std::filesystem::path& source,
                                const std::filesystem::path& archive,
                                std::size_t block_size, std::size_t workers);
    std::string submit_extract(const std::filesystem::path& archive,
                               const std::filesystem::path& destination);
    std::string submit_verify(const std::filesystem::path& archive);

    // Snapshot of a job, or nullptr when the id is unknown.
    std::shared_ptr<Job> get(const std::string& id) const;

    void wait_until_empty();     // for tests

 private:
    void run(std::shared_ptr<Job> job);

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Job>> jobs_;
    std::uint64_t next_id_ = 0;
    std::vector<std::thread> workers_;
};

// Blocking server: listens on 127.0.0.1:port and serves until stop() is
// called (or the object is destroyed). serve_forever() runs its own accept
// loop thread; each request is handled on it, one at a time - the API is a
// local single-user control plane, not a web service.
class ApiServer {
 public:
    explicit ApiServer(std::uint16_t port);
    ~ApiServer();

    void serve_forever();        // spawns the accept loop
    void stop();

    JobManager& jobs() { return job_manager_; }

 private:
    void handle_client(int client_fd);

    std::uint16_t port_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    JobManager job_manager_;
};

}  // namespace uatc::server
