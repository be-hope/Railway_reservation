// result.cpp
// Error catalogue of SAD §4.4.

#include "common/result.h"

namespace rrs {

std::string errorCodeString(ErrorCode code) {
    return "E" + std::to_string(static_cast<int>(code));
}

std::string errorName(ErrorCode code) {
    switch (code) {
        case ErrorCode::None:                return "OK";
        case ErrorCode::Validation:          return "ERR-VALIDATION";
        case ErrorCode::DuplicateUser:       return "ERR-DUP-USER";
        case ErrorCode::AuthInvalid:         return "ERR-AUTH-INVALID";
        case ErrorCode::AccountLocked:       return "ERR-ACCOUNT-LOCKED";
        case ErrorCode::SessionExpired:      return "ERR-SESSION-EXPIRED";
        case ErrorCode::Forbidden:           return "ERR-FORBIDDEN";
        case ErrorCode::TrainNotFound:       return "ERR-TRAIN-NOT-FOUND";
        case ErrorCode::NoCapacity:          return "ERR-NO-CAPACITY";
        case ErrorCode::PnrNotFound:         return "ERR-PNR-NOT-FOUND";
        case ErrorCode::AlreadyCancelled:    return "ERR-ALREADY-CANCELLED";
        case ErrorCode::CancelDeparted:      return "ERR-CANCEL-DEPARTED";
        case ErrorCode::TrainInUse:          return "ERR-TRAIN-IN-USE";
        case ErrorCode::CapacityBelowBooked: return "ERR-CAPACITY-BELOW-BOOKED";
        case ErrorCode::Database:            return "ERR-DB";
        case ErrorCode::Internal:            return "ERR-INTERNAL";
    }
    return "ERR-INTERNAL";
}

std::string errorMessage(ErrorCode code) {
    switch (code) {
        case ErrorCode::None:                return "OK.";
        case ErrorCode::Validation:          return "Invalid input. Please re-enter.";
        case ErrorCode::DuplicateUser:       return "An account with this mobile number or e-mail already exists.";
        case ErrorCode::AuthInvalid:         return "Invalid login ID or password.";
        case ErrorCode::AccountLocked:       return "Too many failed attempts. Try again in 5 minutes.";
        case ErrorCode::SessionExpired:      return "Your session has expired. Please log in again.";
        case ErrorCode::Forbidden:           return "You are not allowed to perform this operation.";
        case ErrorCode::TrainNotFound:       return "Train not found.";
        case ErrorCode::NoCapacity:          return "No seats or waitlist room on this train. Choose another date or class.";
        case ErrorCode::PnrNotFound:         return "PNR not found.";
        case ErrorCode::AlreadyCancelled:    return "This ticket is already cancelled.";
        case ErrorCode::CancelDeparted:      return "The train has already departed; the ticket cannot be cancelled.";
        case ErrorCode::TrainInUse:          return "The train has active reservations and cannot be removed.";
        case ErrorCode::CapacityBelowBooked: return "Capacity cannot be lower than the seats already booked.";
        case ErrorCode::Database:            return "The data could not be saved. Changes were not applied.";
        case ErrorCode::Internal:            return "An unexpected error occurred. Please try again.";
    }
    return "An unexpected error occurred. Please try again.";
}

std::string formatError(const Error& error) {
    std::string line = "ERROR [" + errorCodeString(error.code) + "]: ";
    if (error.code == ErrorCode::Validation && !error.detail.empty()) {
        return line + error.detail + " Please re-enter.";
    }
    line += errorMessage(error.code);
    if (!error.detail.empty() && error.code != ErrorCode::Database &&
        error.code != ErrorCode::Internal) {
        line += " (" + error.detail + ")";
    }
    return line;
}

}  // namespace rrs
