// money.h
// Fixed-point money in paise (SAD §4.1 coding conventions: no floating point
// in fare or refund arithmetic).

#pragma once

#include <cstdint>
#include <string>

namespace rrs {

class Money {
public:
    constexpr Money() = default;
    static constexpr Money fromPaise(std::int64_t paise) { return Money(paise); }
    static constexpr Money fromRupees(std::int64_t rupees) { return Money(rupees * 100); }

    constexpr std::int64_t paise() const { return paise_; }

    // "1,750.00" (Indian rupees, two decimals, thousands separator).
    std::string toString() const;

    constexpr Money operator+(Money o) const { return Money(paise_ + o.paise_); }
    constexpr Money operator-(Money o) const { return Money(paise_ - o.paise_); }
    constexpr Money operator*(std::int64_t n) const { return Money(paise_ * n); }
    Money& operator+=(Money o) { paise_ += o.paise_; return *this; }
    constexpr bool operator==(Money o) const { return paise_ == o.paise_; }
    constexpr bool operator!=(Money o) const { return paise_ != o.paise_; }
    constexpr bool operator<(Money o) const { return paise_ < o.paise_; }

private:
    constexpr explicit Money(std::int64_t paise) : paise_(paise) {}
    std::int64_t paise_ = 0;
};

}  // namespace rrs
