// money.cpp

#include "common/money.h"

namespace rrs {

std::string Money::toString() const {
    std::int64_t abs = paise_ < 0 ? -paise_ : paise_;
    std::string rupees = std::to_string(abs / 100);
    std::string grouped;
    int count = 0;
    for (auto it = rupees.rbegin(); it != rupees.rend(); ++it) {
        if (count > 0 && count % 3 == 0) grouped.insert(grouped.begin(), ',');
        grouped.insert(grouped.begin(), *it);
        ++count;
    }
    std::int64_t frac = abs % 100;
    std::string out = (paise_ < 0 ? "-" : "") + grouped + ".";
    out += static_cast<char>('0' + frac / 10);
    out += static_cast<char>('0' + frac % 10);
    return out;
}

}  // namespace rrs
