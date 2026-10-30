#include "security/input_validator.h"
#include <algorithm>
#include <cctype>

namespace rrs {
namespace {
Status checked(bool valid, const char* field) {
    return valid ? Status::success() : Status::failure(ErrorCode::Validation, std::string("Invalid ") + field + ".");
}
bool digits(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
}
Status InputValidator::mobile(const std::string& s) { return checked(s.size() == 10 && digits(s), "mobile"); }
Status InputValidator::email(const std::string& s) {
    const auto at = s.find('@');
    const auto dot = s.rfind('.');
    bool valid = s.size() <= 100 && at != std::string::npos && at > 0 &&
                 dot != std::string::npos && dot > at + 1 && dot + 1 < s.size() &&
                 s.find('@', at + 1) == std::string::npos;
    for (unsigned char c : s) {
        if (!std::isalnum(c) && c != '@' && c != '.' && c != '_' && c != '+' && c != '-') valid = false;
    }
    return checked(valid, "email");
}
Status InputValidator::password(const std::string& s) {
    bool letter = false, digit = false, printable = true;
    for (unsigned char c : s) {
        letter = letter || std::isalpha(c);
        digit = digit || std::isdigit(c);
        printable = printable && c >= 32 && c != 127;
    }
    return checked(s.size() >= 8 && s.size() <= 128 && letter && digit && printable, "password (8-128 characters, letter and digit required)");
}
Status InputValidator::name(const std::string& s) {
    bool visible = false, valid = !s.empty() && s.size() <= 100;
    for (unsigned char c : s) {
        valid = valid && c >= 32 && c != 127;
        visible = visible || c > 32;
    }
    return checked(valid && visible, "name");
}
Status InputValidator::age(int n) { return checked(n >= 1 && n <= 120, "age"); }
Status InputValidator::gender(char c) { return checked(c == 'M' || c == 'F' || c == 'O', "gender"); }
Status InputValidator::idProof(const std::string& s) {
    bool valid = !s.empty() && s.size() <= 30;
    for (unsigned char c : s) valid = valid && (std::isalnum(c) || c == '-' || c == '/');
    return checked(valid, "ID proof");
}
Status InputValidator::station(const std::string& s) {
    return checked(s.size() >= 2 && s.size() <= 5 && std::all_of(s.begin(), s.end(), [](char c) { return c >= 'A' && c <= 'Z'; }), "station code");
}
Status InputValidator::pnr(const std::string& s) { return checked(s.size() == 10 && s[0] >= '1' && s[0] <= '9' && digits(s), "PNR"); }
Status InputValidator::journeyDate(const Date& d, const Date& today) { return checked(d >= today && d <= today.plusDays(120), "journey date"); }
Status InputValidator::profile(const Account& a) {
    for (const Status& s : {name(a.name), age(a.age), gender(a.gender)}) if (!s.ok()) return s;
    if (!a.contactNumber.empty()) { auto s = mobile(a.contactNumber); if (!s.ok()) return s; }
    if (!a.email.empty()) { auto s = email(a.email); if (!s.ok()) return s; }
    if (a.contactNumber.empty() && a.email.empty()) return checked(false, "contact");
    if (a.idProofType.empty() != a.idProofNumber.empty() || a.idProofType.size() > 20) return checked(false, "ID proof type");
    if (!a.idProofType.empty() && !name(a.idProofType).ok()) return checked(false, "ID proof type");
    if (!a.idProofNumber.empty()) return idProof(a.idProofNumber);
    return Status::success();
}
std::string InputValidator::normalizeEmail(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}  // namespace rrs
