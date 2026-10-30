#include "common/audit.h"
#include <array>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <openssl/evp.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace rrs {
namespace {
std::mutex auditMutex;
std::string sanitize(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (c == '\n' || c == '\r' || c == '|') out += ' ';
        else if (c >= 32 && c != 127) out += static_cast<char>(c);
    }
    return out;
}
std::string digest(const std::string& text) {
    std::array<unsigned char, 32> bytes{};
    unsigned int size = 0;
    if (EVP_Digest(text.data(), text.size(), bytes.data(), &size, EVP_sha256(), nullptr) != 1 || size != bytes.size())
        throw std::runtime_error("Audit digest failed");
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    for (auto c : bytes) { out += hex[c >> 4]; out += hex[c & 15]; }
    return out;
}
std::string verifyChain(const std::string& path) {
    std::ifstream input(path);
    if (!input && std::filesystem::exists(path)) throw std::runtime_error("Cannot read audit log");
    std::string previous(64, '0'), line;
    while (std::getline(input, line)) {
        const auto split = line.rfind(" | hash=");
        if (split == std::string::npos || line.substr(split + 8) != digest(previous + line.substr(0, split)))
            throw std::runtime_error("Audit chain is invalid; preserve the log for investigation");
        previous = line.substr(split + 8);
    }
    if (input.bad()) throw std::runtime_error("Audit read failed");
    return previous;
}
}
bool AppendOnlyFileAuditLogger::verify() const {
    std::lock_guard<std::mutex> guard(auditMutex);
    try { verifyChain(path_); return true; } catch (const std::runtime_error&) { return false; }
}
void AppendOnlyFileAuditLogger::log(const std::string& event, const std::string& actor, const std::string& detail) {
    std::lock_guard<std::mutex> guard(auditMutex);
#ifndef _WIN32
    const int fd = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) throw std::runtime_error("Cannot open audit log");
    const int permissionResult = fchmod(fd, S_IRUSR | S_IWUSR);
    ::close(fd);
    if (permissionResult != 0) throw std::runtime_error("Cannot protect audit log");
#endif
    const auto previous = verifyChain(path_);
    std::time_t t = static_cast<std::time_t>(clock_.now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &utc);
    const std::string line = std::string(stamp) + " | " + sanitize(event) + " | actor=" + sanitize(actor) + " | detail=" + sanitize(detail);
    std::ofstream output(path_, std::ios::app);
    output << line << " | hash=" << digest(previous + line) << '\n';
    output.flush();
    if (!output) throw std::runtime_error("Audit append failed");
}
}
