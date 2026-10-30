// clock.h
// Injectable clock (STP §10: "no test reads the real time").

#pragma once

#include <cstdint>

#include "common/date.h"

namespace rrs {

using EpochSeconds = std::int64_t;

class IClock {
public:
    virtual ~IClock() = default;
    virtual EpochSeconds now() const = 0;   // seconds since the Unix epoch
    virtual int localSecondsSinceMidnight() const { return static_cast<int>((now() % 86400 + 86400) % 86400); }
    virtual Date today() const = 0;         // local calendar date
};

class SystemClock : public IClock {
public:
    EpochSeconds now() const override;
    Date today() const override;
    int localSecondsSinceMidnight() const override;
};

// Test clock: fixed date/time that tests can move forward.
class FixedClock : public IClock {
public:
    FixedClock(Date today, EpochSeconds now) : today_(today), now_(now) {}
    explicit FixedClock(Date today) : today_(today), now_(today.dayNumber() * 86400 + 8 * 3600) {}

    EpochSeconds now() const override { return now_; }
    Date today() const override { return today_; }

    void advanceSeconds(EpochSeconds s) { now_ += s; }
    void setToday(Date d) { today_ = d; }

private:
    Date today_;
    EpochSeconds now_;
};

}  // namespace rrs
