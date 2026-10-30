// audit.h
// Audit-log port (SEC-006, SAD §4.4). Lines look like
//   2026-10-03T14:35:22 | BOOKING_CREATED | actor=<account> | detail=<text>
// Passwords and ID-proof numbers must never be passed in `detail`.
//
// Services depend on IAuditLogger; the append-only file implementation verifies a SHA-256 chain.

#pragma once

#include <string>
#include <utility>

#include "common/clock.h"

namespace rrs {

class IAuditLogger {
public:
    virtual ~IAuditLogger() = default;
    virtual void log(const std::string& eventType, const std::string& actorId,
                     const std::string& detail) = 0;
};

class NullAuditLogger : public IAuditLogger {
public:
    void log(const std::string&, const std::string&, const std::string&) override {}
};

// Appends one line per event to a text file (audit.log).
class AppendOnlyFileAuditLogger : public IAuditLogger {
public:
    AppendOnlyFileAuditLogger(std::string path, const IClock& clock)
        : path_(std::move(path)), clock_(clock) {}
    void log(const std::string& eventType, const std::string& actorId,
             const std::string& detail) override;

    // Detects edited or reordered records. Anchor the final hash externally to detect truncation.
    bool verify() const;

private:
    std::string path_;
    const IClock& clock_;
};

}  // namespace rrs
