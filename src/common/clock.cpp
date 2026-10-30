// clock.cpp

#include "common/clock.h"

#include <chrono>
#include <ctime>

namespace rrs {

EpochSeconds SystemClock::now() const {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

Date SystemClock::today() const {
    std::time_t t = static_cast<std::time_t>(now());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    return *Date::fromYmd(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

int SystemClock::localSecondsSinceMidnight() const {
    std::time_t t = static_cast<std::time_t>(now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    return local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
}
}  // namespace rrs
