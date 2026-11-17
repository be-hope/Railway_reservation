#pragma once
#include "common/domain.h"
#include "common/result.h"

namespace rrs {
class InputValidator {
public:
    static Status mobile(const std::string& value);
    static Status email(const std::string& value);
    static Status password(const std::string& value);
    static Status name(const std::string& value);
    static Status age(int value);
    static Status gender(char value);
    static Status idProof(const std::string& value);
    static Status station(const std::string& value);
    static Status pnr(const std::string& value);
    static Status journeyDate(const Date& value, const Date& today);
    static Status profile(const Account& value);
    static std::string normalizeEmail(std::string value);
};
}  // namespace rrs
