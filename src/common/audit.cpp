// audit.cpp

#include "common/audit.h"

#include <ctime>
#include <fstream>

namespace rrs {

namespace {

// Keeps a log line on one line and free of the field separator.
std::string sanitize(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '\n' || c == '\r' || c == '|') out += ' ';
        else if (static_cast<unsigned char>(c) < 0x20) continue;
        else out += c;
    }
    return out;
}

}  // namespace

void AppendOnlyFileAuditLogger::log(const std::string& eventType, const std::string& actorId,
                                    const std::string& detail) {
    std::time_t t = static_cast<std::time_t>(clock_.now());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &local);

    std::ofstream out(path_, std::ios::app);
    out << stamp << " | " << sanitize(eventType) << " | actor=" << sanitize(actorId)
        << " | detail=" << sanitize(detail) << '\n';
}

}  // namespace rrs
