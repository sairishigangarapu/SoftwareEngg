#include "server/api_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"
#include "core/archive_service.hpp"
#include "core/config.hpp"

namespace uatc::server {

namespace fs = std::filesystem;
using core::ErrorCode;
using core::UatcError;

const char* job_state_name(JobState state) {
    switch (state) {
        case JobState::Queued: return "queued";
        case JobState::Running: return "running";
        case JobState::Completed: return "completed";
        case JobState::Failed: return "failed";
    }
    return "unknown";
}

// ============================================================== errors

const char* user_message(ErrorCode code) {
    // SAD 4.3.4, "Plain-language message (UI)" column, word for word.
    switch (code) {
        case ErrorCode::InvalidArgument:
            return "Something in the request is not valid. Please check the "
                   "file or folder you selected.";
        case ErrorCode::NotFound:
            return "We could not find that file or folder.";
        case ErrorCode::Permission:
            return "UATC is not allowed to open or write that location.";
        case ErrorCode::Io:
            return "The operation stopped because the disk could not be read "
                   "or written (it may be full).";
        case ErrorCode::MalformedArchive:
            return "This file is not a valid UATC archive or is damaged.";
        case ErrorCode::UnsupportedVersion:
            return "This archive was made by a newer version of UATC.";
        case ErrorCode::MissingBlock:
            return "Part of this archive is missing.";
        case ErrorCode::BlockChecksum:
            return "This archive is corrupted and cannot be extracted safely.";
        case ErrorCode::ArchiveChecksum:
            return "This archive failed its integrity check.";
        case ErrorCode::PathTraversal:
            return "The archive contains an unsafe file path and was stopped.";
        case ErrorCode::Internal:
        default:
            return "Something went wrong inside UATC. Please try again.";
    }
}

int http_status_for(ErrorCode code) {
    // SAD 4.3.4, "HTTP (direct call)" column.
    switch (code) {
        case ErrorCode::InvalidArgument: return 400;
        case ErrorCode::NotFound: return 404;
        case ErrorCode::Permission: return 403;
        case ErrorCode::Io: return 500;
        case ErrorCode::MalformedArchive:
        case ErrorCode::UnsupportedVersion:
        case ErrorCode::MissingBlock:
        case ErrorCode::BlockChecksum:
        case ErrorCode::ArchiveChecksum:
        case ErrorCode::PathTraversal:
            return 422;
        case ErrorCode::Internal:
        default:
            return 500;
    }
}

ErrorCode classify_error(const std::exception& error) {
    return core::classify_exception(error);
}

namespace {

std::string json_escape(const std::string& s) {
    std::ostringstream o;
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (ch) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c < 0x20) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                      << static_cast<int>(c) << std::dec;
                } else {
                    o << ch;
                }
        }
    }
    return o.str();
}

}  // namespace

std::string error_json(ErrorCode code, const std::string& message) {
    std::ostringstream o;
    o << "{\"error\":{\"code\":\"" << core::error_code_name(code)
      << "\",\"message\":\"" << json_escape(message)
      << "\",\"userMessage\":\"" << json_escape(user_message(code)) << "\"}}";
    return o.str();
}

// =========================================================== path rule

fs::path resolve_in_root(const fs::path& root, const std::string& requested) {
    if (requested.empty()) {
        throw UatcError(ErrorCode::InvalidArgument, "A path is required.");
    }
    if (requested.find('\0') != std::string::npos) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "The path contains a NUL byte.");
    }

    std::error_code ec;
    const fs::path canonical_root = fs::weakly_canonical(root, ec);
    if (ec) {
        throw UatcError(ErrorCode::Internal, "The allowed root is not usable.");
    }

    fs::path candidate(requested);
    if (candidate.is_relative()) candidate = canonical_root / candidate;

    // weakly_canonical resolves '.', '..' and every symlink in the part of
    // the path that exists, then normalises the rest lexically. Comparing
    // after that is what makes "root/link-to-etc/passwd" and
    // "root/a/../../etc" fail the check.
    const fs::path resolved = fs::weakly_canonical(candidate, ec);
    if (ec) {
        throw UatcError(ErrorCode::Permission,
                        "The path cannot be resolved: " + requested);
    }

    // Component-wise prefix test, so "/data-evil" is not inside "/data".
    auto r = canonical_root.begin();
    auto p = resolved.begin();
    for (; r != canonical_root.end(); ++r, ++p) {
        if (r->empty()) continue;   // trailing separator of the root
        if (p == resolved.end() || *r != *p) {
            throw UatcError(ErrorCode::Permission,
                            "The path is outside the allowed folder: " +
                                requested);
        }
    }
    return resolved;
}

// ================================================================ jobs

JobManager::~JobManager() { wait_until_empty(); }

std::string JobManager::create(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream id;
    id << "j-" << std::setw(4) << std::setfill('0') << ++next_id_;
    Job job;
    job.id = id.str();
    job.operation = operation;
    jobs_[job.id] = job;
    return job.id;
}

template <typename Fn>
void JobManager::update(const std::string& id, Fn&& fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = jobs_.find(id);
    if (it != jobs_.end()) fn(it->second);
}

template <typename Body>
void JobManager::launch(const std::string& id, Body body) {
    std::lock_guard<std::mutex> lock(mutex_);
    workers_.emplace_back([this, id, body] {
        update(id, [](Job& job) { job.state = JobState::Running; });
        const auto started = std::chrono::steady_clock::now();
        try {
            body();
            const auto elapsed = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started).count());
            update(id, [elapsed](Job& job) {
                job.elapsedMs = elapsed;
                job.progress = 1.0;
                job.state = JobState::Completed;
            });
        } catch (const std::exception& error) {
            const auto code = classify_error(error);
            const std::string message = error.what();
            update(id, [code, message](Job& job) {
                job.error = code;
                job.error_message = message;
                job.state = JobState::Failed;
            });
        } catch (...) {
            update(id, [](Job& job) {
                job.error = ErrorCode::Internal;
                job.error_message = "Unknown error.";
                job.state = JobState::Failed;
            });
        }
    });
}

namespace {

// TC-API-03: an unwritable destination must fail the job with
// E_PERMISSION or E_IO, not with whatever the writer happens to throw.
// Runs inside the job, so the failure is reported through the job state.
void check_writable_file_destination(const fs::path& destination) {
    if (fs::is_directory(destination)) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "The destination is a folder, not a file: " +
                            destination.string());
    }
    const fs::path parent = destination.parent_path();
    if (!fs::exists(parent)) {
        throw UatcError(ErrorCode::NotFound,
                        "The destination folder does not exist: " +
                            parent.string());
    }
    if (!fs::is_directory(parent)) {
        throw UatcError(ErrorCode::Io,
                        "The destination cannot be written: " +
                            parent.string() + " is not a folder.");
    }
    // ::access(W_OK) is not reliable on all filesystems: some mounts ignore
    // permission bits or the process has DAC_OVERRIDE. Try to actually
    // create and delete a temp file, which is the only reliable check.
    const auto probe_file = parent / (".uatc_write_probe_" +
                                      std::to_string(::getpid()));
    int probe_fd = ::open(probe_file.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0600);
    if (probe_fd < 0) {
        throw UatcError(ErrorCode::Permission,
                        "No permission to write: " + destination.string());
    }
    ::close(probe_fd);
    ::unlink(probe_file.c_str());
}

void check_writable_directory_destination(const fs::path& destination) {
    // The extractor creates the destination; the nearest existing ancestor
    // must be a writable folder.
    fs::path existing = destination;
    while (!existing.empty() && !fs::exists(existing)) {
        existing = existing.parent_path();
    }
    if (existing.empty() || !fs::is_directory(existing)) {
        throw UatcError(ErrorCode::Io,
                        "The destination cannot be created: " +
                            destination.string());
    }
    if (::access(existing.c_str(), W_OK) != 0) {
        throw UatcError(ErrorCode::Permission,
                        "No permission to write: " + destination.string());
    }
}

std::string transform_display_name(uatc::TransformId id) {
    switch (id) {
        case uatc::TransformId::None: return "None";
        case uatc::TransformId::Delta: return "Delta";
        case uatc::TransformId::Xor: return "XOR";
        case uatc::TransformId::ByteShuffle: return "Byte-shuffle";
        case uatc::TransformId::RLE: return "RLE";
        default: return "Unknown";
    }
}

}  // namespace

std::string JobManager::submit_compress(const fs::path& source,
                                        const fs::path& archive,
                                        std::size_t block_size,
                                        std::size_t workers) {
    const std::string id = create("compress");
    launch(id, [this, id, source, archive, block_size, workers] {
        check_writable_file_destination(archive);

        core::CompressOptions options;
        options.block_size = block_size;
        options.workers = workers;

        class Sink : public core::ProgressSink {
         public:
            Sink(JobManager& manager, std::string job_id)
                : manager_(manager), id_(std::move(job_id)) {}
            void on_block(const core::BlockProgress& p) override {
                const double progress = p.blockCount
                    ? static_cast<double>(p.blockIndex + 1) /
                          static_cast<double>(p.blockCount)
                    : 1.0;
                manager_.update(id_, [progress](Job& job) {
                    // Blocks may report out of order on the parallel path;
                    // the bar never goes backwards.
                    job.progress = std::max(job.progress, progress);
                });
            }
         private:
            JobManager& manager_;
            std::string id_;
        } sink(*this, id);

        // A file or a whole folder (UC-1), through the core facade.
        const auto result = core::compress_path(source, archive, options, &sink);

        update(id, [&result](Job& job) {
            job.files = result.fileCount;
            job.originalBytes = result.originalBytes;
            job.archiveBytes = result.archiveBytes;
            job.ratio = result.ratio;
            job.blocks = result.blockCount;
            job.storedBlocks = result.storedBlocks;
            job.transformUsage.clear();
            for (const auto& [transform, count] : result.transformUsage) {
                job.transformUsage.push_back(
                    {static_cast<int>(transform),
                     transform_display_name(transform), count});
            }
        });
    });
    return id;
}

std::string JobManager::submit_extract(const fs::path& archive,
                                       const fs::path& destination) {
    const std::string id = create("extract");
    launch(id, [this, id, archive, destination] {
        check_writable_directory_destination(destination);
        // Single-file and folder archives alike.
        const auto result = core::extract_path(archive, destination);
        update(id, [&result](Job& job) {
            job.files = result.files;
            job.originalBytes = result.bytesWritten;
        });
    });
    return id;
}

std::string JobManager::submit_verify(const fs::path& archive) {
    const std::string id = create("verify");
    launch(id, [this, id, archive] {
        // Full decode with every CRC32 and the SHA-256 integrity data,
        // single-file or folder archive; the verifier classifies failures.
        const auto verdict = uatc::ArchiveVerifier::verify(archive);
        if (!verdict.success) {
            throw UatcError(verdict.code, verdict.message);
        }
        const auto info = uatc::ArchiveInspector::inspect(archive);
        update(id, [&info](Job& job) {
            job.files = info.fileCount;
            job.originalBytes = info.originalBytes;
            job.archiveBytes = info.archiveBytes;
            job.blocks = info.blockCount;
        });
    });
    return id;
}

std::optional<Job> JobManager::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) return std::nullopt;
    return it->second;
}

void JobManager::wait_until_empty() {
    // Join outside the lock: the job threads take it to report progress.
    std::vector<std::thread> joining;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        joining.swap(workers_);
    }
    for (auto& worker : joining) {
        if (worker.joinable()) worker.join();
    }
}

// ============================================================== server

std::string generate_token() {
    std::random_device device;
    std::ostringstream o;
    for (int i = 0; i < 4; ++i) {
        o << std::hex << std::setw(8) << std::setfill('0')
          << static_cast<std::uint32_t>(device());
    }
    return o.str();
}

namespace {

constexpr std::size_t kMaxHeaderBytes = 16 * 1024;
constexpr std::size_t kMaxBodyBytes = 64 * 1024;
constexpr std::size_t kMaxWorkers = 256;
constexpr std::size_t kMinBlockSize = 1024;               // 1 KiB
constexpr std::size_t kMaxBlockSize = 64 * 1024 * 1024;   // 64 MiB

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;   // lower-case names
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string body;
    std::string content_type = "application/json";
};

[[noreturn]] void fail(ErrorCode code, const std::string& message) {
    throw UatcError(code, message);
}

const char* reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 422: return "Unprocessable Entity";
        case 431: return "Request Header Fields Too Large";
        default: return "Internal Server Error";
    }
}

void send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent,
                                 MSG_NOSIGNAL);
        if (n <= 0) return;
        sent += static_cast<std::size_t>(n);
    }
}

void send_response(int fd, const HttpResponse& response,
                   const std::string& allow_origin) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << " "
        << reason_phrase(response.status) << "\r\n"
        << "Content-Type: " << response.content_type << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "X-Content-Type-Options: nosniff\r\n"
        << "Cache-Control: no-store\r\n";
    // CORS: echo the origin only when it is on the allow-list. Never '*'.
    if (!allow_origin.empty()) {
        out << "Access-Control-Allow-Origin: " << allow_origin << "\r\n"
            << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            << "Access-Control-Allow-Headers: Content-Type, X-UATC-Token\r\n"
            << "Vary: Origin\r\n";
    }
    out << "Connection: close\r\n\r\n" << response.body;
    send_all(fd, out.str());
}

HttpResponse json_error(ErrorCode code, const std::string& message) {
    return {http_status_for(code), error_json(code, message)};
}

// Reads one request with the size caps. Returns 0 on success, otherwise
// the HTTP status to answer with (or -1 to drop the connection silently).
int read_request(int fd, std::string& raw, std::size_t& header_end) {
    char buffer[4096];
    header_end = std::string::npos;
    while (header_end == std::string::npos) {
        const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
        if (got <= 0) return -1;
        raw.append(buffer, static_cast<std::size_t>(got));
        header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos && raw.size() > kMaxHeaderBytes) {
            return 431;
        }
    }
    if (header_end > kMaxHeaderBytes) return 431;
    return 0;
}

std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t");
    return s.substr(begin, end - begin + 1);
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Strict percent-decoding (TC-SEC-04): a '%' must be followed by two hex
// digits. Decoding happens exactly once, so "%252e%252e" stays "%2e%2e"
// and is never re-interpreted as "..".
std::string percent_decode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%') {
            if (i + 2 >= value.size()) {
                fail(ErrorCode::InvalidArgument, "Bad percent-encoding.");
            }
            const int hi = hex_value(value[i + 1]);
            const int lo = hex_value(value[i + 2]);
            if (hi < 0 || lo < 0) {
                fail(ErrorCode::InvalidArgument, "Bad percent-encoding.");
            }
            out.push_back(static_cast<char>(hi * 16 + lo));
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
    while (pos <= query.size()) {
        const auto amp = query.find('&', pos);
        const std::string pair = query.substr(
            pos, amp == std::string::npos ? std::string::npos : amp - pos);
        const auto eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == name) {
            return percent_decode(pair.substr(eq + 1));
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

// ------------------------------------------------- minimal JSON object
//
// The API bodies are flat objects of strings and numbers. This parser
// accepts exactly that and rejects everything else (nesting, duplicate
// keys, raw control characters, trailing garbage) with E_INVALID_ARGUMENT.

struct JsonValue {
    bool is_string = false;
    std::string text;
};
using JsonObject = std::map<std::string, JsonValue>;

class FlatJsonParser {
 public:
    explicit FlatJsonParser(const std::string& s) : s_(s) {}

    JsonObject parse() {
        JsonObject out;
        skip_ws();
        expect('{');
        skip_ws();
        if (peek() == '}') {
            ++i_;
        } else {
            for (;;) {
                skip_ws();
                const std::string key = parse_string();
                skip_ws();
                expect(':');
                skip_ws();
                JsonValue value;
                if (peek() == '"') {
                    value.is_string = true;
                    value.text = parse_string();
                } else {
                    value.text = parse_scalar();
                }
                if (!out.emplace(key, value).second) {
                    bad("duplicate field \"" + key + "\"");
                }
                skip_ws();
                if (peek() == ',') { ++i_; continue; }
                expect('}');
                break;
            }
        }
        skip_ws();
        if (i_ != s_.size()) bad("unexpected text after the object");
        return out;
    }

 private:
    [[noreturn]] void bad(const std::string& why) {
        fail(ErrorCode::InvalidArgument, "Invalid JSON body: " + why + ".");
    }
    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
    void expect(char c) {
        if (peek() != c) bad(std::string("expected '") + c + "'");
        ++i_;
    }
    void skip_ws() {
        while (i_ < s_.size() &&
               (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' ||
                s_[i_] == '\r')) {
            ++i_;
        }
    }
    void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    unsigned parse_hex4() {
        if (i_ + 4 > s_.size()) bad("short \\u escape");
        unsigned value = 0;
        for (int k = 0; k < 4; ++k) {
            const int h = hex_value(s_[i_++]);
            if (h < 0) bad("bad \\u escape");
            value = value * 16 + static_cast<unsigned>(h);
        }
        return value;
    }
    std::string parse_string() {
        expect('"');
        std::string out;
        for (;;) {
            if (i_ >= s_.size()) bad("unterminated string");
            const char c = s_[i_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) bad("control character in string");
            if (c != '\\') { out.push_back(c); continue; }
            if (i_ >= s_.size()) bad("unterminated escape");
            const char e = s_[i_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = parse_hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (peek() != '\\') bad("lone surrogate");
                        ++i_;
                        expect('u');
                        const unsigned low = parse_hex4();
                        if (low < 0xDC00 || low > 0xDFFF) bad("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        bad("lone surrogate");
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: bad("unknown escape");
            }
        }
    }
    std::string parse_scalar() {
        const std::size_t start = i_;
        while (i_ < s_.size() &&
               (std::isalnum(static_cast<unsigned char>(s_[i_])) ||
                s_[i_] == '-' || s_[i_] == '+' || s_[i_] == '.')) {
            ++i_;
        }
        if (i_ == start) bad("expected a value");
        return s_.substr(start, i_ - start);
    }

    const std::string& s_;
    std::size_t i_ = 0;
};

// A required string field.
std::string string_field(const JsonObject& body, const std::string& name) {
    const auto it = body.find(name);
    if (it == body.end() || !it->second.is_string || it->second.text.empty()) {
        fail(ErrorCode::InvalidArgument,
             "The field \"" + name + "\" is required and must be a string.");
    }
    return it->second.text;
}

// An optional whole number in [low, high].
std::size_t number_field(const JsonObject& body, const std::string& name,
                         std::size_t fallback, std::size_t low,
                         std::size_t high) {
    const auto it = body.find(name);
    if (it == body.end() || (!it->second.is_string && it->second.text == "null")) {
        return fallback;
    }
    const std::string& text = it->second.text;
    const bool digits = !it->second.is_string && !text.empty() &&
                        text.size() <= 12 &&
                        std::all_of(text.begin(), text.end(), [](char c) {
                            return std::isdigit(static_cast<unsigned char>(c));
                        });
    if (!digits) {
        fail(ErrorCode::InvalidArgument,
             "The field \"" + name + "\" must be a whole number.");
    }
    const std::size_t value = std::stoull(text);
    if (value < low || value > high) {
        fail(ErrorCode::InvalidArgument,
             "The field \"" + name + "\" must be between " +
                 std::to_string(low) + " and " + std::to_string(high) + ".");
    }
    return value;
}

// Constant-time comparison so the token cannot be guessed byte by byte
// from response timing.
bool same_token(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string job_json(const Job& job) {
    std::ostringstream o;
    o << "{\"jobId\":\"" << job.id << "\",\"operation\":\"" << job.operation
      << "\",\"state\":\"" << job_state_name(job.state)
      << "\",\"progress\":" << job.progress;
    if (job.state == JobState::Failed) {
        const std::string error = error_json(job.error, job.error_message);
        // error_json is {"error":{...}}; splice its member into this object.
        o << "," << error.substr(1, error.size() - 2);
    } else if (job.state == JobState::Completed) {
        const double seconds = static_cast<double>(job.elapsedMs) / 1000.0;
        const double throughput =
            seconds > 0.0
                ? static_cast<double>(job.originalBytes) / (1024.0 * 1024.0) /
                      seconds
                : 0.0;
        o << ",\"result\":{\"originalBytes\":" << job.originalBytes
          << ",\"archiveBytes\":" << job.archiveBytes
          << ",\"ratio\":" << job.ratio
          << ",\"elapsedMs\":" << job.elapsedMs
          << ",\"throughputMBps\":" << throughput
          << ",\"files\":" << job.files
          << ",\"blocks\":" << job.blocks
          << ",\"stored\":" << job.storedBlocks;
        if (job.operation == "verify") o << ",\"valid\":true";
        o << ",\"transformUsage\":[";
        for (std::size_t i = 0; i < job.transformUsage.size(); ++i) {
            const auto& t = job.transformUsage[i];
            o << (i ? "," : "") << "{\"id\":" << t.id << ",\"name\":\""
              << json_escape(t.name) << "\",\"blocks\":" << t.blocks << "}";
        }
        o << "]}";
    }
    o << "}";
    return o.str();
}

std::string info_json(const ArchiveInfo& info, const fs::path& path) {
    std::ostringstream o;
    o << "{\"path\":\"" << json_escape(path.string())
      << "\",\"formatVersion\":{\"major\":" << info.formatVersion
      << ",\"minor\":0}"
      << ",\"fileCount\":" << info.fileCount
      << ",\"blockCount\":" << info.blockCount
      << ",\"originalBytes\":" << info.originalBytes
      << ",\"archiveBytes\":" << info.archiveBytes
      << ",\"ratio\":" << info.ratio
      << ",\"storedBlocks\":" << info.storedBlocks
      << ",\"compressedBlocks\":" << info.compressedBlocks
      << ",\"transformUsage\":[";
    for (std::size_t i = 0; i < info.transformUsage.size(); ++i) {
        const auto& t = info.transformUsage[i];
        o << (i ? "," : "") << "{\"id\":" << static_cast<int>(t.id)
          << ",\"name\":\"" << json_escape(t.name)
          << "\",\"blocks\":" << t.blocks << "}";
    }
    o << "],\"codecUsage\":[";
    for (std::size_t i = 0; i < info.codecUsage.size(); ++i) {
        const auto& c = info.codecUsage[i];
        o << (i ? "," : "") << "{\"id\":" << static_cast<int>(c.id)
          << ",\"name\":\"" << json_escape(c.name)
          << "\",\"blocks\":" << c.blocks << "}";
    }
    // The file list, capped so a huge archive cannot produce a huge reply.
    constexpr std::size_t kMaxListedFiles = 500;
    o << "],\"files\":[";
    const std::size_t listed = std::min(info.files.size(), kMaxListedFiles);
    for (std::size_t i = 0; i < listed; ++i) {
        const auto& f = info.files[i];
        o << (i ? "," : "") << "{\"path\":\"" << json_escape(f.path)
          << "\",\"originalBytes\":" << f.originalBytes
          << ",\"blocks\":" << f.blocks << "}";
    }
    o << "],\"filesTruncated\":"
      << (info.files.size() > listed ? "true" : "false") << "}";
    return o.str();
}

// The static frontend files the server may serve. A fixed list, so no
// request path ever reaches the filesystem directly.
const std::map<std::string, std::pair<std::string, std::string>>&
static_files() {
    static const std::map<std::string, std::pair<std::string, std::string>>
        files = {
            {"/", {"index.html", "text/html; charset=utf-8"}},
            {"/index.html", {"index.html", "text/html; charset=utf-8"}},
            {"/app.js", {"app.js", "text/javascript; charset=utf-8"}},
            {"/styles.css", {"styles.css", "text/css; charset=utf-8"}},
        };
    return files;
}

}  // namespace

ApiServer::ApiServer(std::uint16_t port)
    : ApiServer([port] {
          ServerOptions options;
          options.port = port;
          return options;
      }()) {}

ApiServer::ApiServer(ServerOptions options) : options_(std::move(options)) {
    if (options_.token.empty()) options_.token = generate_token();
    std::error_code ec;
    options_.allowed_root = fs::weakly_canonical(options_.allowed_root, ec);
    if (ec || !fs::is_directory(options_.allowed_root)) {
        throw std::runtime_error("The allowed root is not a folder: " +
                                 options_.allowed_root.string());
    }
    // The server's own origin is always allowed (frontend served by us).
    const std::string port = std::to_string(options_.port);
    options_.allowed_origins.push_back("http://127.0.0.1:" + port);
    options_.allowed_origins.push_back("http://localhost:" + port);

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
    address.sin_port = htons(options_.port);

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

ApiServer::~ApiServer() {
    stop();
    if (listen_fd_ >= 0) ::close(listen_fd_);
}

void ApiServer::serve_forever() {
    // Blocking: the accept loop runs on the calling thread until stop()
    // shuts the socket down under accept().
    running_ = true;
    while (running_) {
        const int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0) {
            if (!running_) break;   // stopped: the shutdown path
            continue;               // transient: try again
        }
        // A client that connects and goes quiet must not block the
        // single-threaded server forever.
        timeval timeout{};
        timeout.tv_sec = 5;
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        handle_client(client);
        ::close(client);
    }
}

void ApiServer::stop() {
    if (!running_.exchange(false)) return;
    ::shutdown(listen_fd_, SHUT_RDWR);
}

void ApiServer::handle_client(int client_fd) {
    std::string raw;
    std::size_t header_end = 0;
    const int read_status = read_request(client_fd, raw, header_end);
    if (read_status < 0) return;
    if (read_status > 0) {
        send_response(client_fd,
                      {read_status, error_json(ErrorCode::InvalidArgument,
                                               "The request headers are too large.")},
                      "");
        return;
    }

    HttpRequest request;
    std::string allow_origin;

    auto respond = [&](const HttpResponse& response) {
        send_response(client_fd, response, allow_origin);
    };

    try {
        // ---- request line and headers
        const auto line_end = raw.find("\r\n");
        const std::string request_line = raw.substr(0, line_end);
        const auto space1 = request_line.find(' ');
        const auto space2 = request_line.find(' ', space1 == std::string::npos
                                                       ? 0 : space1 + 1);
        if (space1 == std::string::npos || space2 == std::string::npos) {
            fail(ErrorCode::InvalidArgument, "Malformed request line.");
        }
        request.method = request_line.substr(0, space1);
        const std::string target =
            request_line.substr(space1 + 1, space2 - space1 - 1);
        const auto qmark = target.find('?');
        request.path = target.substr(0, qmark);
        request.query = qmark == std::string::npos ? "" : target.substr(qmark + 1);

        std::size_t pos = line_end + 2;
        while (pos < header_end) {
            const auto next = raw.find("\r\n", pos);
            const std::string line = raw.substr(pos, next - pos);
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                request.headers[to_lower(trim(line.substr(0, colon)))] =
                    trim(line.substr(colon + 1));
            }
            pos = next + 2;
        }

        // ---- body, capped
        std::size_t content_length = 0;
        if (const auto it = request.headers.find("content-length");
            it != request.headers.end()) {
            const std::string& v = it->second;
            if (v.empty() || v.size() > 9 ||
                !std::all_of(v.begin(), v.end(), [](char c) {
                    return std::isdigit(static_cast<unsigned char>(c));
                })) {
                fail(ErrorCode::InvalidArgument, "Bad Content-Length.");
            }
            content_length = std::stoul(v);
            if (content_length > kMaxBodyBytes) {
                respond({413, error_json(ErrorCode::InvalidArgument,
                                         "The request body is too large.")});
                return;
            }
        }
        request.body = raw.substr(header_end + 4);
        char buffer[4096];
        while (request.body.size() < content_length) {
            const ssize_t got = ::recv(client_fd, buffer, sizeof(buffer), 0);
            if (got <= 0) break;
            request.body.append(buffer, static_cast<std::size_t>(got));
        }
        if (request.body.size() < content_length) {
            fail(ErrorCode::InvalidArgument, "The request body is incomplete.");
        }
        request.body.resize(content_length);

        // ---- TC-SEC-03: Host, Origin and token checks
        //
        // Host: a page on another site that rebinds its DNS name to
        // 127.0.0.1 still sends its own name in Host. Only loopback names
        // are accepted.
        {
            const std::string host = to_lower(request.headers["host"]);
            const std::string port = ":" + std::to_string(options_.port);
            const bool loopback =
                host == "127.0.0.1" || host == "localhost" ||
                host == "127.0.0.1" + port || host == "localhost" + port;
            if (!loopback) {
                respond(json_error(ErrorCode::Permission,
                                   "Requests must be addressed to 127.0.0.1."));
                return;
            }
        }
        // Origin: browsers always send it on cross-origin requests. A
        // request with no Origin is a non-browser client (curl, tests),
        // which the token alone authenticates.
        if (const auto it = request.headers.find("origin");
            it != request.headers.end()) {
            const auto& allowed = options_.allowed_origins;
            if (std::find(allowed.begin(), allowed.end(), it->second) ==
                allowed.end()) {
                respond(json_error(ErrorCode::Permission,
                                   "This origin may not use the UATC API."));
                return;
            }
            allow_origin = it->second;
        }

        // CORS preflight carries no token by design; answer it once the
        // origin has passed the check above.
        if (request.method == "OPTIONS") {
            respond({204, ""});
            return;
        }

        // ---- static frontend (no token: the files hold no data)
        if (request.method == "GET" && !options_.web_root.empty()) {
            const auto& files = static_files();
            if (const auto it = files.find(request.path); it != files.end()) {
                std::ifstream in(options_.web_root / it->second.first,
                                 std::ios::binary);
                if (!in) fail(ErrorCode::NotFound, "Frontend file missing.");
                std::ostringstream content;
                content << in.rdbuf();
                respond({200, content.str(), it->second.second});
                return;
            }
        }

        // GET /api/health - availability check, no token, no data.
        if (request.method == "GET" && request.path == "/api/health") {
            respond({200, "{\"status\":\"ok\",\"version\":\"1.0\"}"});
            return;
        }

        if (request.path.rfind("/api/", 0) != 0) {
            fail(ErrorCode::NotFound, "Unknown endpoint: " + request.path);
        }

        // Token: required on every other /api call.
        if (!same_token(request.headers["x-uatc-token"], options_.token)) {
            respond(json_error(ErrorCode::Permission,
                               "Missing or wrong X-UATC-Token header."));
            return;
        }

        const auto& root = options_.allowed_root;

        // ---- GET /api/fs/list?path=
        if (request.method == "GET" && request.path == "/api/fs/list") {
            const std::string requested = query_param(request.query, "path");
            const fs::path dir =
                resolve_in_root(root, requested.empty() ? root.string() : requested);
            if (!fs::exists(dir)) {
                fail(ErrorCode::NotFound, "No such folder: " + requested);
            }
            if (!fs::is_directory(dir)) {
                fail(ErrorCode::InvalidArgument, "Not a folder: " + requested);
            }
            struct Entry { std::string name; bool directory; std::uintmax_t size; };
            std::vector<Entry> entries;
            std::error_code ec;
            for (fs::directory_iterator it(
                     dir, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec)) {
                std::error_code entry_ec;
                const bool directory = it->is_directory(entry_ec);
                const bool regular = it->is_regular_file(entry_ec);
                entries.push_back({it->path().filename().string(), directory,
                                   regular ? it->file_size(entry_ec) : 0});
            }
            if (ec) fail(ErrorCode::Permission, "Cannot list: " + requested);
            std::sort(entries.begin(), entries.end(),
                      [](const Entry& a, const Entry& b) { return a.name < b.name; });

            std::ostringstream o;
            o << "{\"path\":\"" << json_escape(dir.string())
              << "\",\"root\":\"" << json_escape(root.string())
              << "\",\"entries\":[";
            for (std::size_t i = 0; i < entries.size(); ++i) {
                o << (i ? "," : "") << "{\"name\":\""
                  << json_escape(entries[i].name) << "\",\"type\":\""
                  << (entries[i].directory ? "directory" : "file")
                  << "\",\"size\":" << entries[i].size << "}";
            }
            o << "]}";
            respond({200, o.str()});
            return;
        }

        // ---- GET /api/archives/info?path=  (FR-11)
        if (request.method == "GET" && request.path == "/api/archives/info") {
            const fs::path archive =
                resolve_in_root(root, query_param(request.query, "path"));
            if (!fs::is_regular_file(archive)) {
                fail(ErrorCode::NotFound, "No such archive.");
            }
            const auto info = uatc::ArchiveInspector::inspect(archive);
            respond({200, info_json(info, archive)});
            return;
        }

        // ---- GET /api/jobs/{id}
        if (request.method == "GET" &&
            request.path.rfind("/api/jobs/", 0) == 0 &&
            request.path.size() > std::strlen("/api/jobs/")) {
            const auto job =
                job_manager_.get(request.path.substr(std::strlen("/api/jobs/")));
            if (!job) fail(ErrorCode::NotFound, "Unknown job id.");
            respond({200, job_json(*job)});
            return;
        }

        if (request.method == "POST") {
            const JsonObject body = FlatJsonParser(request.body).parse();

            // ---- POST /api/jobs/compress
            if (request.path == "/api/jobs/compress") {
                const fs::path source =
                    resolve_in_root(root, string_field(body, "source"));
                const fs::path destination =
                    resolve_in_root(root, string_field(body, "destination"));
                const std::size_t workers =
                    number_field(body, "workers", 1, 1, kMaxWorkers);
                const std::size_t block_size =
                    number_field(body, "blockSize", core::kDefaultBlockSize,
                                 kMinBlockSize, kMaxBlockSize);
                if (!fs::exists(source)) {
                    fail(ErrorCode::NotFound, "No such source.");
                }
                const auto id = job_manager_.submit_compress(
                    source, destination, block_size, workers);
                respond({202, "{\"jobId\":\"" + id + "\"}"});
                return;
            }

            // ---- POST /api/jobs/extract
            if (request.path == "/api/jobs/extract") {
                const fs::path archive =
                    resolve_in_root(root, string_field(body, "archive"));
                const fs::path destination =
                    resolve_in_root(root, string_field(body, "destination"));
                if (!fs::exists(archive)) {
                    fail(ErrorCode::NotFound, "No such archive.");
                }
                const auto id = job_manager_.submit_extract(archive, destination);
                respond({202, "{\"jobId\":\"" + id + "\"}"});
                return;
            }

            // ---- POST /api/jobs/verify
            if (request.path == "/api/jobs/verify") {
                const fs::path archive =
                    resolve_in_root(root, string_field(body, "archive"));
                if (!fs::exists(archive)) {
                    fail(ErrorCode::NotFound, "No such archive.");
                }
                const auto id = job_manager_.submit_verify(archive);
                respond({202, "{\"jobId\":\"" + id + "\"}"});
                return;
            }
        }

        fail(ErrorCode::NotFound, "Unknown endpoint: " + request.method + " " +
                                      request.path);
    } catch (const std::exception& error) {
        // Every failure becomes a SAD error object; the server never dies
        // on a bad request.
        const ErrorCode code = classify_error(error);
        respond(json_error(code, error.what()));
    }
}

}  // namespace uatc::server
