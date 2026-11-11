// HTTP API layer and job manager tests (FR-13/14/15).
//
// Boots the real server on a fixed loopback port, drives it with raw HTTP
// over a socket, and checks the contract in docs/api-contract.md and the
// security rules of SAD 3.9.
//
// STP test cases covered here:
//   TC-API-01  compress job: 202 + jobId, polled to completed, with stats
//   TC-API-02  missing / invalid fields -> 400 with an error object
//   TC-API-03  unwritable destination -> job failed with E_IO/E_PERMISSION
//   TC-API-04  fs/list outside the allowed root -> 403 E_PERMISSION
//   TC-SEC-03  wrong Host / wrong Origin / missing or wrong token -> 403
//   TC-SEC-04  '..', percent-encoded '..', bad encodings, NUL, symlink
//              escape, absurd numbers -> refused, server keeps running
//   FR-11      archives/info returns the inspection fields
//   SAD 4.3.4  every error carries code, message and userMessage
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "core/error.hpp"
#include "server/api_server.hpp"

namespace fs = std::filesystem;
using uatc::core::ErrorCode;

namespace {

constexpr std::uint16_t kPort = 18099;
const std::string kHost = "Host: 127.0.0.1:18099\r\n";

int failures = 0;

void check(bool ok, const std::string& name, const std::string& detail = "") {
    if (ok) {
        std::cout << "PASS: " << name << "\n";
    } else {
        std::cerr << "FAIL: " << name << (detail.empty() ? "" : "  -- " + detail)
                  << "\n";
        ++failures;
    }
}

std::string http(const std::string& request) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(kPort);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(fd);
        throw std::runtime_error("connect - is the server up?");
    }
    ::send(fd, request.data(), request.size(), MSG_NOSIGNAL);

    std::string response;
    char buffer[4096];
    ssize_t got;
    while ((got = ::recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, static_cast<std::size_t>(got));
    }
    ::close(fd);
    return response;
}

std::string body_of(const std::string& response) {
    const auto at = response.find("\r\n\r\n");
    return at == std::string::npos ? "" : response.substr(at + 4);
}

std::string headers_of(const std::string& response) {
    return response.substr(0, response.find("\r\n\r\n"));
}

int status_of(const std::string& response) {
    std::istringstream first(response.substr(0, response.find("\r\n")));
    std::string version;
    int status = 0;
    first >> version >> status;
    return status;
}

void write_file(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << data;
}

// A tiny JSON string-field reader, the same shape the server emits.
std::string field(const std::string& json, const std::string& name) {
    const std::string needle = "\"" + name + "\":\"";
    const auto at = json.find(needle);
    if (at == std::string::npos) return "";
    const auto start = at + needle.size();
    const auto end = json.find('"', start);
    return json.substr(start, end - start);
}

bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// SAD 4.3.4: an error object always has all three members.
bool is_error_object(const std::string& body, const std::string& code) {
    return field(body, "code") == code && has(body, "\"message\":\"") &&
           !field(body, "userMessage").empty();
}

std::string percent_encode(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '/' ||
            c == '-' || c == '_' || c == '.') {
            out += c;
        } else {
            char buf[8];
            std::snprintf(buf, sizeof buf, "%%%02X",
                          static_cast<unsigned char>(c));
            out += buf;
        }
    }
    return out;
}

}  // namespace

int main() {
    using namespace uatc;

    const fs::path work = fs::absolute("uatc_api_test_dir");
    fs::remove_all(work);
    fs::create_directories(work / "root" / "sub");
    const fs::path root = fs::canonical(work / "root");

    // --- Corpus, all inside the allowed root
    const auto source = root / "sample.txt";
    write_file(source, "the quick brown fox jumps over the lazy dog. ");
    {
        std::ofstream big(source, std::ios::binary | std::ios::app);
        for (int i = 0; i < 2000; ++i) big << "line " << i << " of the log file.\n";
    }
    write_file(work / "outside.txt", "secret");   // outside the root
    fs::create_directory_symlink(work, root / "escape");   // link out of the root

    // ================================================ unit: path rule
    {
        using server::resolve_in_root;
        auto code_of = [&](const std::string& p) {
            try {
                resolve_in_root(root, p);
                return std::string("OK");
            } catch (const core::UatcError& e) {
                return std::string(core::error_code_name(e.code()));
            }
        };
        check(code_of("sample.txt") == "OK", "path: relative inside root");
        check(code_of(source.string()) == "OK", "path: absolute inside root");
        check(code_of("sub/new.uatc") == "OK", "path: new file inside root");
        check(code_of("../outside.txt") == "E_PERMISSION", "path: '..' escape refused");
        check(code_of("sub/../../outside.txt") == "E_PERMISSION",
              "path: nested '..' escape refused");
        check(code_of("/etc/passwd") == "E_PERMISSION", "path: absolute outside refused");
        check(code_of("escape/outside.txt") == "E_PERMISSION",
              "path: symlink escape refused");
        check(code_of(root.string() + "-evil/x") == "E_PERMISSION",
              "path: sibling with same prefix refused");
        check(code_of("") == "E_INVALID_ARGUMENT", "path: empty refused");
        check(code_of(std::string("a\0b", 3)) == "E_INVALID_ARGUMENT",
              "path: NUL byte refused");
    }

    // ============================================ unit: error mapping
    {
        using server::classify_error;
        check(classify_error(core::UatcError(ErrorCode::PathTraversal, "x")) ==
                  ErrorCode::PathTraversal, "classify: UatcError keeps its code");
        check(classify_error(std::runtime_error("Block checksum mismatch.")) ==
                  ErrorCode::BlockChecksum, "classify: block checksum");
        check(classify_error(std::runtime_error("Archive checksum mismatch.")) ==
                  ErrorCode::ArchiveChecksum, "classify: archive checksum");
        check(classify_error(std::runtime_error("Unsupported UATC archive version.")) ==
                  ErrorCode::UnsupportedVersion, "classify: version");
        check(classify_error(std::runtime_error("Invalid UATC archive magic.")) ==
                  ErrorCode::MalformedArchive, "classify: bad magic");
        check(classify_error(std::runtime_error(
                  "The archive has no integrity footer: it is not a valid UATC "
                  "archive or was written by an unsupported version.")) ==
                  ErrorCode::MalformedArchive, "classify: no footer is malformed");
        check(classify_error(fs::filesystem_error(
                  "x", std::make_error_code(std::errc::permission_denied))) ==
                  ErrorCode::Permission, "classify: EACCES -> E_PERMISSION");
        check(classify_error(fs::filesystem_error(
                  "x", std::make_error_code(std::errc::no_space_on_device))) ==
                  ErrorCode::Io, "classify: ENOSPC -> E_IO");
        check(server::http_status_for(ErrorCode::BlockChecksum) == 422 &&
                  server::http_status_for(ErrorCode::Permission) == 403 &&
                  server::http_status_for(ErrorCode::InvalidArgument) == 400,
              "SAD 4.3.4 HTTP status column");
    }

    // ================================================ boot the server
    server::ServerOptions options;
    options.port = kPort;
    options.allowed_root = root;
    server::ApiServer server(options);
    const std::string token = server.token();
    const std::string auth = "X-UATC-Token: " + token + "\r\n";
    check(token.size() == 32, "token: 128-bit random token generated");
    check(server::generate_token() != token, "token: differs per session");

    std::thread server_thread([&server] { server.serve_forever(); });

    auto get = [&](const std::string& target, const std::string& extra = "") {
        return http("GET " + target + " HTTP/1.1\r\n" + kHost + auth + extra + "\r\n");
    };
    auto post = [&](const std::string& target, const std::string& body,
                    const std::string& extra = "") {
        return http("POST " + target + " HTTP/1.1\r\n" + kHost + auth + extra +
                    "Content-Length: " + std::to_string(body.size()) +
                    "\r\n\r\n" + body);
    };
    auto poll = [&](const std::string& id) {
        std::string body;
        for (int attempt = 0; attempt < 200; ++attempt) {
            body = body_of(get("/api/jobs/" + id));
            const auto state = field(body, "state");
            if (state == "completed" || state == "failed") break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        return body;
    };

    // ---- health: reachable without a token
    {
        const auto r = http("GET /api/health HTTP/1.1\r\n" + kHost + "\r\n");
        check(status_of(r) == 200 && has(body_of(r), "\"status\":\"ok\""),
              "health: 200 without token");
    }

    // ================================================== TC-SEC-03
    {
        const auto r = http("GET /api/fs/list HTTP/1.1\r\n" + kHost + "\r\n");
        check(status_of(r) == 403 && is_error_object(body_of(r), "E_PERMISSION"),
              "TC-SEC-03: missing token -> 403 E_PERMISSION", body_of(r));
    }
    {
        const auto r = http("GET /api/fs/list HTTP/1.1\r\n" + kHost +
                            "X-UATC-Token: " + std::string(32, '0') + "\r\n\r\n");
        check(status_of(r) == 403, "TC-SEC-03: wrong token -> 403");
    }
    {
        const auto r = http("GET /api/fs/list HTTP/1.1\r\nHost: evil.example:18099\r\n" +
                            auth + "\r\n");
        check(status_of(r) == 403, "TC-SEC-03: non-loopback Host (DNS rebinding) -> 403");
    }
    {
        const auto r = get("/api/fs/list", "Origin: https://evil.example\r\n");
        check(status_of(r) == 403 &&
                  !has(headers_of(r), "Access-Control-Allow-Origin"),
              "TC-SEC-03: foreign Origin -> 403, no CORS header");
    }
    {
        const auto r = get("/api/fs/list", "Origin: http://localhost:8000\r\n");
        check(status_of(r) == 200 &&
                  has(headers_of(r),
                      "Access-Control-Allow-Origin: http://localhost:8000"),
              "TC-SEC-03: allowed Origin is echoed back");
        check(!has(headers_of(r), "Access-Control-Allow-Origin: *"),
              "TC-SEC-03: never a wildcard origin");
    }
    {
        const auto r = http("OPTIONS /api/jobs/compress HTTP/1.1\r\n" + kHost +
                            "Origin: http://localhost:8000\r\n\r\n");
        check(status_of(r) == 204 && has(headers_of(r), "X-UATC-Token"),
              "TC-SEC-03: preflight from allowed origin -> 204");
        const auto bad = http("OPTIONS /api/jobs/compress HTTP/1.1\r\n" + kHost +
                              "Origin: https://evil.example\r\n\r\n");
        check(status_of(bad) == 403, "TC-SEC-03: preflight from foreign origin -> 403");
    }

    // ============================================ fs/list, TC-API-04
    {
        const auto r = get("/api/fs/list?path=" + percent_encode(root.string()));
        const auto b = body_of(r);
        check(status_of(r) == 200 && has(b, "\"name\":\"sample.txt\"") &&
                  has(b, "\"name\":\"sub\",\"type\":\"directory\""),
              "fs/list: inside root", b);
        const auto def = get("/api/fs/list");
        check(status_of(def) == 200 && has(body_of(def), "sample.txt"),
              "fs/list: no path lists the root");
    }
    {
        const auto r = get("/api/fs/list?path=" + percent_encode(work.string()));
        check(status_of(r) == 403 && is_error_object(body_of(r), "E_PERMISSION") &&
                  !has(body_of(r), "outside.txt"),
              "TC-API-04: outside root -> 403, nothing listed", body_of(r));
    }
    {
        const auto r = get("/api/fs/list?path=/etc");
        check(status_of(r) == 403, "TC-API-04: /etc -> 403");
    }

    // ================================================== TC-SEC-04
    {
        check(status_of(get("/api/fs/list?path=..")) == 403,
              "TC-SEC-04: '..' -> 403");
        check(status_of(get("/api/fs/list?path=%2e%2e")) == 403,
              "TC-SEC-04: percent-encoded '..' -> 403");
        check(status_of(get("/api/fs/list?path=sub%2f..%2f..")) == 403,
              "TC-SEC-04: encoded 'sub/../..' -> 403");
        check(status_of(get("/api/fs/list?path=escape")) == 403,
              "TC-SEC-04: symlink out of root -> 403");
        // Double encoding decodes once to the literal name "%2e%2e",
        // which does not exist inside the root: 404, never a listing.
        check(status_of(get("/api/fs/list?path=%252e%252e")) == 404,
              "TC-SEC-04: double encoding is not decoded twice");
        const auto bad = get("/api/fs/list?path=%zz");
        check(status_of(bad) == 400 && is_error_object(body_of(bad), "E_INVALID_ARGUMENT"),
              "TC-SEC-04: malformed percent-encoding -> 400");
        check(status_of(get("/api/fs/list?path=a%00b")) == 400,
              "TC-SEC-04: NUL byte -> 400");
        const auto traversal = post("/api/jobs/compress",
                                    "{\"source\":\"../outside.txt\",\"destination\":\"x.uatc\"}");
        check(status_of(traversal) == 403,
              "TC-SEC-04: job source outside root -> 403");
        const auto dest_out = post("/api/jobs/compress",
                                   "{\"source\":\"sample.txt\",\"destination\":\"/tmp/x.uatc\"}");
        check(status_of(dest_out) == 403,
              "TC-SEC-04: job destination outside root -> 403");
    }

    // ================================================== TC-API-02
    {
        auto expect_400 = [&](const std::string& body, const std::string& name) {
            const auto r = post("/api/jobs/compress", body);
            check(status_of(r) == 400 &&
                      is_error_object(body_of(r), "E_INVALID_ARGUMENT"),
                  "TC-API-02: " + name, body_of(r));
        };
        expect_400("{}", "missing fields");
        expect_400("{\"source\":\"sample.txt\"}", "missing destination");
        expect_400("not json", "body is not JSON");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\",\"workers\":\"8\"}",
                   "workers as a string");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\",\"workers\":0}",
                   "workers = 0");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\",\"workers\":99999999999}",
                   "workers absurdly large");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\",\"workers\":-1}",
                   "workers negative");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\",\"blockSize\":1}",
                   "blockSize below 1 KiB");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\",\"blockSize\":1e9}",
                   "blockSize not a whole number");
        expect_400("{\"source\":\"sample.txt\",\"source\":\"x\",\"destination\":\"o.uatc\"}",
                   "duplicate field");
        expect_400("{\"source\":{\"a\":1},\"destination\":\"o.uatc\"}",
                   "nested object");
        expect_400("{\"source\":\"sample.txt\",\"destination\":\"o.uatc\"} trailing",
                   "trailing garbage");

        const auto huge = post("/api/jobs/compress", std::string(70 * 1024, ' '));
        check(status_of(huge) == 413, "TC-API-02: body over 64 KiB -> 413");

        const auto missing = post("/api/jobs/compress",
                                  "{\"source\":\"nope.txt\",\"destination\":\"o.uatc\"}");
        check(status_of(missing) == 404 && is_error_object(body_of(missing), "E_NOT_FOUND"),
              "TC-API-02: missing source -> 404 E_NOT_FOUND");
    }

    // ================================================== TC-API-01
    std::string archive_path = (root / "sample.uatc").string();
    {
        const auto r = post("/api/jobs/compress",
                            "{\"source\":\"sample.txt\",\"destination\":\"sample.uatc\","
                            "\"blockSize\":4096,\"workers\":2}");
        const auto id = field(body_of(r), "jobId");
        check(status_of(r) == 202 && !id.empty(), "TC-API-01: 202 with jobId");
        const auto job = poll(id);
        check(field(job, "state") == "completed", "TC-API-01: job completed", job);
        check(has(job, "\"originalBytes\":") && has(job, "\"archiveBytes\":") &&
                  has(job, "\"ratio\":") && has(job, "\"elapsedMs\":") &&
                  has(job, "\"throughputMBps\":") && has(job, "\"blocks\":") &&
                  has(job, "\"stored\":"),
              "TC-API-01: result carries the SAD statistics", job);
        check(has(job, "\"transformUsage\":[{\"id\":") && has(job, "\"name\":\""),
              "TC-API-01: transformUsage has numeric id and name", job);
        check(fs::exists(archive_path), "TC-API-01: archive written");
    }

    // ================================================== TC-API-03
    {
        // The destination's parent is a file: it can never be written.
        const auto r = post("/api/jobs/compress",
                            "{\"source\":\"sample.txt\","
                            "\"destination\":\"sample.txt/out.uatc\"}");
        const auto job = poll(field(body_of(r), "jobId"));
        check(status_of(r) == 202 && field(job, "state") == "failed" &&
                  (field(job, "code") == "E_IO" || field(job, "code") == "E_PERMISSION") &&
                  !field(job, "userMessage").empty(),
              "TC-API-03: unwritable destination -> failed E_IO/E_PERMISSION", job);
    }
    if (::geteuid() != 0) {
        // Permission bits are ignored for root, so only check them as a user.
        fs::create_directories(root / "locked");
        fs::permissions(root / "locked", fs::perms::owner_read | fs::perms::owner_exec);
        // Some filesystems ignore permission bits. Skip if still writable.
        if (::access((root / "locked").c_str(), W_OK) == 0) {
            std::cout << "SKIP: TC-API-03 read-only folder (fs ignores perms)\n";
        } else {
            const auto r = post("/api/jobs/compress",
                                "{\"source\":\"sample.txt\","
                                "\"destination\":\"locked/out.uatc\"}");
            const auto job = poll(field(body_of(r), "jobId"));
            check(field(job, "code") == "E_PERMISSION",
                  "TC-API-03: read-only folder -> E_PERMISSION", job);
        }
        fs::permissions(root / "locked", fs::perms::owner_all);
    } else {
        std::cout << "SKIP: TC-API-03 read-only folder (running as root)\n";
    }
    {
        const auto r = post("/api/jobs/compress",
                            "{\"source\":\"sample.txt\","
                            "\"destination\":\"no/such/dir/out.uatc\"}");
        const auto job = poll(field(body_of(r), "jobId"));
        check(field(job, "code") == "E_NOT_FOUND",
              "TC-API-03: missing destination folder -> E_NOT_FOUND", job);
    }

    // ===================================== failed jobs carry SAD errors
    {
        // A file that exists but is not an archive.
        const auto r = post("/api/jobs/extract",
                            "{\"archive\":\"sample.txt\",\"destination\":\"restored\"}");
        const auto job = poll(field(body_of(r), "jobId"));
        check(field(job, "state") == "failed" &&
                  is_error_object(job, "E_MALFORMED_ARCHIVE"),
              "extract: non-archive -> failed E_MALFORMED_ARCHIVE + userMessage", job);
    }
    {
        const auto r = post("/api/jobs/extract",
                            "{\"archive\":\"sample.uatc\",\"destination\":\"restored\"}");
        const auto job = poll(field(body_of(r), "jobId"));
        check(field(job, "state") == "completed", "extract: real archive completes", job);
    }

    // ================================================== verify
    {
        const auto r = post("/api/jobs/verify", "{\"archive\":\"sample.uatc\"}");
        const auto job = poll(field(body_of(r), "jobId"));
        check(field(job, "state") == "completed" && has(job, "\"valid\":true"),
              "verify: intact archive -> valid", job);
    }

    // ================================================== FR-11 info
    {
        const auto r = get("/api/archives/info?path=sample.uatc");
        const auto b = body_of(r);
        check(status_of(r) == 200 && has(b, "\"fileCount\":1") &&
                  has(b, "\"blockCount\":") && has(b, "\"storedBlocks\":") &&
                  has(b, "\"compressedBlocks\":") &&
                  has(b, "\"formatVersion\":{\"major\":") &&
                  has(b, "\"transformUsage\":[{\"id\":") && has(b, "\"codecUsage\":["),
              "FR-11: archives/info returns the inspection fields", b);
        const auto bad = get("/api/archives/info?path=sample.txt");
        check(status_of(bad) == 422 &&
                  is_error_object(body_of(bad), "E_MALFORMED_ARCHIVE"),
              "FR-11: info on a non-archive -> 422 E_MALFORMED_ARCHIVE", body_of(bad));
        check(status_of(get("/api/archives/info?path=..%2Foutside.txt")) == 403,
              "FR-11: info outside root -> 403");
        check(status_of(get("/api/archives/info?path=missing.uatc")) == 404,
              "FR-11: info on a missing file -> 404");
    }

    // ======================================= UC-1: a folder via the API
    {
        write_file(root / "project" / "a.txt", "alpha alpha alpha alpha\n");
        write_file(root / "project" / "src" / "b.txt", std::string(9000, 'b'));
        const auto r = post("/api/jobs/compress",
                            "{\"source\":\"project\",\"destination\":\"project.uatc\"}");
        const auto job = poll(field(body_of(r), "jobId"));
        check(field(job, "state") == "completed" && has(job, "\"files\":2"),
              "UC-1: folder compress job completes with 2 files", job);

        const auto info = body_of(get("/api/archives/info?path=project.uatc"));
        check(has(info, "\"fileCount\":2") &&
                  has(info, "{\"path\":\"a.txt\"") &&
                  has(info, "{\"path\":\"src/b.txt\"") &&
                  has(info, "\"filesTruncated\":false"),
              "FR-11: info lists the files of a folder archive", info);

        const auto v = poll(field(body_of(post("/api/jobs/verify",
                                               "{\"archive\":\"project.uatc\"}")),
                                  "jobId"));
        check(field(v, "state") == "completed" && has(v, "\"valid\":true"),
              "TC-VER-01: folder archive verifies through the API", v);

        const auto x = poll(field(body_of(post("/api/jobs/extract",
                                               "{\"archive\":\"project.uatc\","
                                               "\"destination\":\"project_out\"}")),
                                  "jobId"));
        check(field(x, "state") == "completed" && has(x, "\"files\":2") &&
                  fs::exists(root / "project_out" / "src" / "b.txt"),
              "UC-1: folder archive extracts through the API", x);
    }

    // ================================================== misc
    {
        const auto r = get("/api/jobs/j-9999");
        check(status_of(r) == 404 && is_error_object(body_of(r), "E_NOT_FOUND"),
              "unknown job -> 404 E_NOT_FOUND with userMessage");
        check(status_of(get("/api/nope")) == 404, "unknown endpoint -> 404");
        const auto garbage = http("garbage\r\n\r\n");
        check(status_of(garbage) == 400, "malformed request line -> 400");
        // The server is still alive after all of the above.
        check(status_of(http("GET /api/health HTTP/1.1\r\n" + kHost + "\r\n")) == 200,
              "server survives every bad request");
    }

    // The job threads must be joined before the server is destroyed.
    server.jobs().wait_until_empty();
    server.stop();
    server_thread.join();
    fs::remove_all(work);

    if (failures != 0) {
        std::printf("api_test: %d FAILURE(S)\n", failures);
        return 1;
    }
    std::printf("api_test: PASS\n");
    return 0;
}
