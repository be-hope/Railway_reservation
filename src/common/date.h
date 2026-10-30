// date.h
// Calendar date used for journey dates. Stored as ISO "YYYY-MM-DD" (sorts
// correctly in SQLite); shown to users as DD-MM-YYYY (SRS §5.2).

#pragma once

#include <optional>
#include <string>

namespace rrs {

class Date {
public:
    Date() = default;
    // Returns nullopt for impossible dates such as 31-02-2026.
    static std::optional<Date> fromYmd(int year, int month, int day);
    static std::optional<Date> parseIso(const std::string& text);       // YYYY-MM-DD
    static std::optional<Date> parseDisplay(const std::string& text);   // DD-MM-YYYY
    static Date fromDayNumber(long days);                               // days since 1970-01-01

    int year() const { return year_; }
    int month() const { return month_; }
    int day() const { return day_; }

    long dayNumber() const;            // days since 1970-01-01
    Date plusDays(long days) const { return fromDayNumber(dayNumber() + days); }

    std::string iso() const;           // 2026-11-05
    std::string display() const;       // 05-11-2026

    bool operator==(const Date& o) const { return dayNumber() == o.dayNumber(); }
    bool operator!=(const Date& o) const { return !(*this == o); }
    bool operator<(const Date& o) const { return dayNumber() < o.dayNumber(); }
    bool operator<=(const Date& o) const { return dayNumber() <= o.dayNumber(); }
    bool operator>(const Date& o) const { return o < *this; }
    bool operator>=(const Date& o) const { return o <= *this; }

private:
    Date(int y, int m, int d) : year_(y), month_(m), day_(d) {}
    int year_ = 1970;
    int month_ = 1;
    int day_ = 1;
};

}  // namespace rrs
