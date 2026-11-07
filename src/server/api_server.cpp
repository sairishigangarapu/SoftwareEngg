#include "server/api_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <chrono>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "archive/archive_inspector.hpp"
#include "archive/archive_reader.hpp"
#include "core/error.hpp"
#include "core/path_safety.hpp"

namespace uatc::server {

const char* job_state_name(JobState state) {
    switch (state) {
        case JobState::Queued: return "queued";
        case JobState::Running: return "running";
        case JobState::Completed: return "completed";
        case JobState::Failed: return "failed";
    }
    return "unknown";
}

// ---------------------------------------------------------------- jobs

std::string JobManager::submit_compress(
    const std::filesystem::path& source,
    const std::filesystem::path& archive,
    std::size_t block_size, std::size_t workers) {
    auto job = std::make_shared<Job>();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream id;
        id << "j-" << std::setw(4) << std::setfill('0') << ++next_id_;
        job->id = id.str();
        job->operation = "compress";
        jobs_[job->id] = job;
    }
    workers_.emplace_back([this, job, source, archive, block_size, workers] {
        job->state = JobState::Running;
        try {
            core::CompressOptions options;
            options.block_size = block_size;
            options.workers = workers;

            class Sink : public core::ProgressSink {
             public:
                explicit Sink(Job& j) : job_(j) {}
                void on_block(const core::BlockProgress& p) override {
                    job_.progress = p.blockCount
                        ? static_cast<double>(p.blockIndex + 1) /
                              static_cast<double>(p.blockCount)
                        : 1.0;
                }
             private:
                Job& job_;
            } sink(*job);

            const auto started = std::chrono::steady_clock::now();
            const auto result = core::CompressionManager::compress(
                source, archive, options, &sink);
            job->originalBytes = result.originalBytes;
            job->archiveBytes = result.archiveBytes;
            job->ratio = result.ratio;
            for (const auto& [id, count] : result.transformUsage) {
                job->transformUsage.emplace_back(
                    std::to_string(static_cast<int>(id)), count);
            }
            job->elapsedMs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started).count());
            job->progress = 1.0;
            job->state = JobState::Completed;
        } catch (const core::UatcError& error) {
            job->error_code = core::error_code_name(error.code());
            job->user_message = error.what();
            job->state = JobState::Failed;
        } catch (const std::exception& error) {
            job->error_code = "E_INTERNAL";
            job->user_message = error.what();
            job->state = JobState::Failed;
        }
    });
    return job->id;
}

std::string JobManager::submit_extract(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination) {
    auto job = std::make_shared<Job>();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream id;
        id << "j-" << std::setw(4) << std::setfill('0') << ++next_id_;
        job->id = id.str();
        job->operation = "extract";
        jobs_[job->id] = job;
    }
    workers_.emplace_back([job, archive, destination] {
        job->state = JobState::Running;
        try {
            const auto result =
                core::CompressionManager::extract(archive, destination);
            job->archiveBytes = result.bytesWritten;
            job->progress = 1.0;
            job->state = JobState::Completed;
        } catch (const core::UatcError& error) {
            job->error_code = core::error_code_name(error.code());
            job->user_message = error.what();
            job->state = JobState::Failed;
        } catch (const std::exception& error) {
            job->error_code = "E_INTERNAL";
            job->user_message = error.what();
            job->state = JobState::Failed;
        }
    });
    return job->id;
}

std::string JobManager::submit_verify(const std::filesystem::path& archive) {
    auto job = std::make_shared<Job>();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream id;
        id << "j-" << std::setw(4) << std::setfill('0') << ++next_id_;
        job->id = id.str();
        job->operation = "verify";
        jobs_[job->id] = job;
    }
    workers_.emplace_back([job, archive] {
        job->state = JobState::Running;
        try {
            const auto info = uatc::ArchiveInspector::inspect(archive);
            (void)uatc::ArchiveReader::read(archive);  // full verification
            job->originalBytes = info.originalBytes;
            job->archiveBytes = info.archiveBytes;
            job->progress = 1.0;
            job->state = JobState::Completed;
        } catch (const std::exception& error) {
            job->error_code = "E_MALFORMED_ARCHIVE";
            job->user_message = error.what();
            job->state = JobState::Failed;
        }
    });
    return job->id;
}

std::shared_ptr<Job> JobManager::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = jobs_.find(id);
    return it == jobs_.end() ? nullptr : it->second;
}

void JobManager::wait_until_empty() {
    // Jobs are fire-and-forget threads; poll until every job is terminal.
    for (;;) {
        bool busy = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [id, job] : jobs_) {
                if (job->state == JobState::Queued ||
                    job->state == JobState::Running) {
                    busy = true;
                    break;
                }
            }
        }
        if (!busy) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

// --------------------------------------------------------------- server

namespace {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
};

bool read_all(int fd, std::string& out) {
    char buffer[4096];
    std::string request;
    std::size_t content_length = 0;
    std::size_t header_end = std::string::npos;

    while (header_end == std::string::npos) {
        const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
        if (got <= 0) return false;
        request.append(buffer, static_cast<std::size_t>(got));
        header_end = request.find("\r\n\r\n");
    }

    // Content-Length, case-insensitive.
    std::size_t pos = 0;
    while (true) {
        const auto line_end = request.find("\r\n", pos);
        if (line_end == std::string::npos || line_end == pos) break;
        const std::string line = request.substr(pos, line_end - pos);
        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            for (auto& c : name) c = static_cast<char>(std::tolower(c));
            if (name == "content-length") content_length = std::stoul(value);
        }
        pos = line_end + 2;
    }

    while (request.size() < header_end + 4 + content_length) {
        const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
        if (got <= 0) break;
        request.append(buffer, static_cast<std::size_t>(got));
    }

    out = request;
    return true;
}

void send_response(int fd, int status, const std::string& body) {
    const char* reason = status == 200 ? "OK"
        : status == 202 ? "Accepted"
        : status == 400 ? "Bad Request"
        : status == 404 ? "Not Found"
        : status == 501 ? "Not Implemented"
        : "Internal Server Error";
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " " << reason << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             // The frontend is served from a static server on another
             // port; the API is localhost-only (SAD 3.9), so a permissive
             // origin header cannot expose it beyond the machine.
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    const std::string out = response.str();
    ::send(fd, out.data(), static_cast<ssize_t>(out.size()), 0);
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if (c != '\r' && c != '\t') o += c;
    }
    return o;
}

std::string percent_decode(const std::string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            out.push_back(static_cast<char>(std::stoul(value.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        } else if (value[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::string query_param(const std::string& query, const std::string& name) {
    std::size_t pos = 0;
    while (pos < query.size()) {
        const auto amp = query.find('&', pos);
        const std::string pair =
            query.substr(pos, amp == std::string::npos ? std::string::npos
                                                       : amp - pos);
        const auto eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == name) {
            return percent_decode(pair.substr(eq + 1));
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

std::string job_json(const Job& job) {
    std::ostringstream o;
    o << "{\"jobId\":\"" << job.id << "\",\"operation\":\"" << job.operation
      << "\",\"state\":\"" << job_state_name(job.state)
      << "\",\"progress\":" << job.progress;
    if (job.state == JobState::Failed) {
        o << ",\"error\":{\"code\":\"" << job.error_code
          << "\",\"message\":\"" << json_escape(job.user_message)
          << "\",\"userMessage\":\"" << json_escape(job.user_message) << "\"}";
    } else if (job.state == JobState::Completed) {
        o << ",\"result\":{\"originalBytes\":" << job.originalBytes
          << ",\"archiveBytes\":" << job.archiveBytes
          << ",\"ratio\":" << job.ratio
          << ",\"elapsedMs\":" << job.elapsedMs
          << ",\"transformUsage\":[";
        for (std::size_t i = 0; i < job.transformUsage.size(); ++i) {
            o << (i ? "," : "") << "{\"id\":\"" << job.transformUsage[i].first
              << "\",\"blocks\":" << job.transformUsage[i].second << "}";
        }
        o << "]}";
    }
    o << "}";
    return o.str();
}

std::string body_field(const std::string& body, const std::string& name) {
    // A minimal string-field extraction: "name":"value".
    const std::string needle = "\"" + name + "\":\"";
    const auto at = body.find(needle);
    if (at == std::string::npos) return "";
    const auto start = at + needle.size();
    const auto end = body.find('"', start);
    if (end == std::string::npos) return "";
    return body.substr(start, end - start);
}

std::string number_field(const std::string& body, const std::string& name) {
    const std::string needle = "\"" + name + "\":";
    const auto at = body.find(needle);
    if (at == std::string::npos) return "";
    const auto start = at + needle.size();
    const auto end = body.find_first_of(",}", start);
    if (end == std::string::npos) return "";
    return body.substr(start, end - start);
}

}  // namespace

ApiServer::ApiServer(std::uint16_t port) : port_(port) {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("socket() failed");
    }
    int reuse = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    // SAD 3.9: localhost only. The API is a local control plane.
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port_);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) < 0) {
        ::close(listen_fd_);
        throw std::runtime_error("bind() failed - is another server running?");
    }
    if (::listen(listen_fd_, 8) < 0) {
        ::close(listen_fd_);
        throw std::runtime_error("listen() failed");
    }
}

ApiServer::~ApiServer() { stop(); }

void ApiServer::serve_forever() {
    // Blocking: the accept loop runs on the calling thread until stop()
    // closes the socket out from under accept(). Callers that need to keep
    // working wrap this in their own thread.
    running_ = true;
    while (running_) {
        const int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0) {
            if (!running_) break;   // stopped: the shutdown path
            continue;               // transient: try again
        }
        handle_client(client);
        ::close(client);
    }
}

void ApiServer::stop() {
    if (!running_.exchange(false)) return;
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
}

void ApiServer::handle_client(int client_fd) {
    std::string raw;
    if (!read_all(client_fd, raw)) return;

    HttpRequest request;
    {
        const auto space1 = raw.find(' ');
        if (space1 == std::string::npos) { send_response(client_fd, 400, "{}"); return; }
        request.method = raw.substr(0, space1);
        const auto space2 = raw.find(' ', space1 + 1);
        const std::string target = raw.substr(space1 + 1, space2 - space1 - 1);
        const auto qmark = target.find('?');
        request.path = qmark == std::string::npos ? target : target.substr(0, qmark);
        request.query = qmark == std::string::npos ? "" : target.substr(qmark + 1);
        const auto body_at = raw.find("\r\n\r\n");
        request.body = body_at == std::string::npos ? "" : raw.substr(body_at + 4);
    }

    // CORS preflight for the browser frontend.
    if (request.method == "OPTIONS") {
        send_response(client_fd, 204, "");
        return;
    }

    // GET /api/health
    if (request.method == "GET" && request.path == "/api/health") {
        send_response(client_fd, 200,
                      "{\"status\":\"ok\",\"version\":\"1.0\"}");
        return;
    }

    // GET /api/fs/list?path=
    if (request.method == "GET" && request.path == "/api/fs/list") {
        const std::string root = query_param(request.query, "path");
        if (root.empty() || !std::filesystem::exists(root) ||
            !std::filesystem::is_directory(root)) {
            send_response(client_fd, 404,
                          "{\"error\":{\"code\":\"E_NOT_FOUND\"}}");
            return;
        }
        std::ostringstream o;
        o << "{\"path\":\"" << json_escape(root) << "\",\"entries\":[";
        bool first = true;
        for (const auto& entry :
             std::filesystem::directory_iterator(root)) {
            if (!first) o << ",";
            first = false;
            o << "{\"name\":\"" << json_escape(entry.path().filename().string())
              << "\",\"type\":\""
              << (entry.is_directory() ? "directory" : "file")
              << "\",\"size\":"
              << (entry.is_regular_file() ? std::filesystem::file_size(entry.path()) : 0)
              << "}";
        }
        o << "]}";
        send_response(client_fd, 200, o.str());
        return;
    }

    // POST /api/jobs/compress
    if (request.method == "POST" && request.path == "/api/jobs/compress") {
        const std::string source = body_field(request.body, "source");
        const std::string destination = body_field(request.body, "destination");
        const std::string workers_str = number_field(request.body, "workers");
        const std::string block_str = number_field(request.body, "blockSize");
        if (source.empty() || destination.empty()) {
            send_response(client_fd, 400,
                          "{\"error\":{\"code\":\"E_INVALID_ARGUMENT\","
                          "\"message\":\"source and destination are required\"}}");
            return;
        }
        if (!std::filesystem::exists(source)) {
            send_response(client_fd, 404,
                          "{\"error\":{\"code\":\"E_NOT_FOUND\"}}");
            return;
        }
        const auto id = job_manager_.submit_compress(
            source, destination,
            block_str.empty() ? core::kDefaultBlockSize : std::stoul(block_str),
            workers_str.empty() ? 1 : std::stoul(workers_str));
        send_response(client_fd, 202, "{\"jobId\":\"" + id + "\"}");
        return;
    }

    // POST /api/jobs/extract
    if (request.method == "POST" && request.path == "/api/jobs/extract") {
        const std::string archive = body_field(request.body, "archive");
        const std::string destination = body_field(request.body, "destination");
        if (archive.empty() || destination.empty()) {
            send_response(client_fd, 400,
                          "{\"error\":{\"code\":\"E_INVALID_ARGUMENT\"}}");
            return;
        }
        if (!std::filesystem::exists(archive)) {
            send_response(client_fd, 404,
                          "{\"error\":{\"code\":\"E_NOT_FOUND\"}}");
            return;
        }
        const auto id =
            job_manager_.submit_extract(archive, destination);
        send_response(client_fd, 202, "{\"jobId\":\"" + id + "\"}");
        return;
    }

    // POST /api/jobs/verify
    if (request.method == "POST" && request.path == "/api/jobs/verify") {
        const std::string archive = body_field(request.body, "archive");
        if (archive.empty() || !std::filesystem::exists(archive)) {
            send_response(client_fd, 404,
                          "{\"error\":{\"code\":\"E_NOT_FOUND\"}}");
            return;
        }
        const auto id = job_manager_.submit_verify(archive);
        send_response(client_fd, 202, "{\"jobId\":\"" + id + "\"}");
        return;
    }

    // GET /api/jobs/{id}
    if (request.method == "GET" &&
        request.path.rfind("/api/jobs/", 0) == 0 &&
        request.path.size() > std::strlen("/api/jobs/")) {
        const std::string id = request.path.substr(strlen("/api/jobs/"));
        const auto job = job_manager_.get(id);
        if (job == nullptr) {
            send_response(client_fd, 404,
                          "{\"error\":{\"code\":\"E_NOT_FOUND\"}}");
            return;
        }
        send_response(client_fd, 200, job_json(*job));
        return;
    }

    // GET /api/archives/info?path=
    if (request.method == "GET" && request.path == "/api/archives/info") {
        const std::string archive = query_param(request.query, "path");
        try {
            const auto info = uatc::ArchiveInspector::inspect(archive);
            std::ostringstream o;
            o << "{\"sizes\":{\"original\":" << info.originalBytes
              << ",\"archive\":" << info.archiveBytes
              << "},\"counts\":{\"files\":" << info.fileCount
              << ",\"blocks\":" << info.blockCount
              << "},\"ratio\":" << info.ratio << "}";
            send_response(client_fd, 200, o.str());
        } catch (const std::exception&) {
            send_response(client_fd, 404,
                          "{\"error\":{\"code\":\"E_NOT_FOUND\"}}");
        }
        return;
    }

    send_response(client_fd, 501, "{\"error\":{\"code\":\"E_NOT_IMPLEMENTED\"}}");
}

}  // namespace uatc::server
