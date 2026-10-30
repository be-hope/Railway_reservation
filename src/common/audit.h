// audit.h
// Audit-log port (SEC-006, SAD §4.4). Lines look like
//   2026-10-03T14:35:22 | BOOKING_CREATED | actor=<account> | detail=<text>
// Passwords and ID-proof numbers must never be passed in `detail`.
//
// NOTE: the full AuditLogger is issue #19; services depend only on IAuditLogger
// so that implementation can replace AppendOnlyFileAuditLogger without changes.

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

private:
    std::string path_;
    const IClock& clock_;
};

}  // namespace rrs
