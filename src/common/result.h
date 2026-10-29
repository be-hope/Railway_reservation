// result.h
// Result<T> and the error-code catalogue of SAD §4.4 (ADR-003).
// Errors are values, not exceptions: every service operation returns a Result.
//
// NOTE: minimal version needed by the booking and data-access modules.
// Issue #17 (project skeleton, Result<T> and error codes) may extend it;
// keep the codes and their numbers stable (API contract rules, SAD §4.3).

#pragma once

#include <optional>
#include <string>
#include <utility>

namespace rrs {

enum class ErrorCode {
    None = 0,
    Validation = 1001,            // ERR-VALIDATION
    DuplicateUser = 1002,         // ERR-DUP-USER
    AuthInvalid = 1003,           // ERR-AUTH-INVALID
    AccountLocked = 1004,         // ERR-ACCOUNT-LOCKED
    SessionExpired = 1005,        // ERR-SESSION-EXPIRED
    Forbidden = 1006,             // ERR-FORBIDDEN
    TrainNotFound = 2001,         // ERR-TRAIN-NOT-FOUND
    NoCapacity = 3001,            // ERR-NO-CAPACITY
    PnrNotFound = 4001,           // ERR-PNR-NOT-FOUND
    AlreadyCancelled = 4002,      // ERR-ALREADY-CANCELLED
    CancelDeparted = 4003,        // ERR-CANCEL-DEPARTED
    TrainInUse = 5001,            // ERR-TRAIN-IN-USE
    CapacityBelowBooked = 5002,   // ERR-CAPACITY-BELOW-BOOKED
    Database = 9001,              // ERR-DB
    Internal = 9999               // ERR-INTERNAL
};

// "E3001" style code used in user messages and logs.
std::string errorCodeString(ErrorCode code);
// "ERR-NO-CAPACITY" style name.
std::string errorName(ErrorCode code);
// Standard user message from the SAD §4.4 catalogue.
std::string errorMessage(ErrorCode code);

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string detail;   // field-level reason; never contains secrets
};

// Formats "ERROR [E3001]: <message> <detail>" for the console (SRS §3.1).
std::string formatError(const Error& error);

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)) {}           // NOLINT: implicit by design
    Result(Error error) : error_(std::move(error)) {}       // NOLINT: implicit by design

    static Result failure(ErrorCode code, std::string detail = {}) {
        return Result(Error{code, std::move(detail)});
    }

    bool ok() const { return value_.has_value(); }
    explicit operator bool() const { return ok(); }

    const T& value() const& { return *value_; }
    T& value() & { return *value_; }
    T&& value() && { return std::move(*value_); }
    const T* operator->() const { return &*value_; }
    T* operator->() { return &*value_; }

    const Error& error() const { return error_; }
    ErrorCode code() const { return ok() ? ErrorCode::None : error_.code; }

private:
    std::optional<T> value_;
    Error error_;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)), ok_(false) {}   // NOLINT

    static Result success() { return Result(); }
    static Result failure(ErrorCode code, std::string detail = {}) {
        return Result(Error{code, std::move(detail)});
    }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const Error& error() const { return error_; }
    ErrorCode code() const { return ok_ ? ErrorCode::None : error_.code; }

private:
    Error error_;
    bool ok_ = true;
};

using Status = Result<void>;

}  // namespace rrs
