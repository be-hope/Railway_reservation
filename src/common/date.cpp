// date.cpp
// Civil-date arithmetic (H. Hinnant's days_from_civil / civil_from_days).

#include "common/date.h"

#include <cctype>
#include <cstdio>

namespace rrs {

namespace {

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : kDays[m - 1];
}

// Parses exactly `width` digits starting at `pos`.
bool readNumber(const std::string& s, std::size_t pos, std::size_t width, int& out) {
    if (pos + width > s.size()) return false;
    int value = 0;
    for (std::size_t i = pos; i < pos + width; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        value = value * 10 + (s[i] - '0');
    }
    out = value;
    return true;
}

}  // namespace

std::optional<Date> Date::fromYmd(int year, int month, int day) {
    if (year < 1900 || year > 9999 || month < 1 || month > 12) return std::nullopt;
    if (day < 1 || day > daysInMonth(year, month)) return std::nullopt;
    return Date(year, month, day);
}

std::optional<Date> Date::parseIso(const std::string& text) {
    int y = 0, m = 0, d = 0;
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
    if (!readNumber(text, 0, 4, y) || !readNumber(text, 5, 2, m) || !readNumber(text, 8, 2, d)) {
        return std::nullopt;
    }
    return fromYmd(y, m, d);
}

std::optional<Date> Date::parseDisplay(const std::string& text) {
    int y = 0, m = 0, d = 0;
    if (text.size() != 10 || text[2] != '-' || text[5] != '-') return std::nullopt;
    if (!readNumber(text, 0, 2, d) || !readNumber(text, 3, 2, m) || !readNumber(text, 6, 4, y)) {
        return std::nullopt;
    }
    return fromYmd(y, m, d);
}

long Date::dayNumber() const {
    int y = year_ - (month_ <= 2 ? 1 : 0);
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long mp = (month_ + 9) % 12;
    const long doy = (153 * mp + 2) / 5 + day_ - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Date Date::fromDayNumber(long days) {
    days += 719468;
    const long era = (days >= 0 ? days : days - 146096) / 146097;
    const long doe = days - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    const int y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
    return Date(y, m, d);
}

std::string Date::iso() const {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", year_, month_, day_);
    return buf;
}

std::string Date::display() const {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d-%02d-%04d", day_, month_, year_);
    return buf;
}

}  // namespace rrs
