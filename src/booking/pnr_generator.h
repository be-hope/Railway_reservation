// pnr_generator.h
// Unique 10-digit PNR numbers (FR-014, UT-BKG-03). No I/O: uniqueness is
// checked through the `exists` callback, so tests can stub it (NFR-006).

#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <string>

#include "common/result.h"

namespace rrs {

class PnrGenerator {
public:
    static constexpr int kMaxAttempts = 5;
    using RandomSource = std::function<std::uint64_t()>;
    using ExistsCheck = std::function<Result<bool>(const std::string&)>;

    // Uses a std::mt19937_64 seeded from std::random_device.
    PnrGenerator();
    // Deterministic source for tests.
    explicit PnrGenerator(RandomSource random) : random_(std::move(random)) {}
    // The default source refers to engine_, so copies would share stale state.
    PnrGenerator(const PnrGenerator&) = delete;
    PnrGenerator& operator=(const PnrGenerator&) = delete;

    // Draws until a PNR not reported by `exists` is found (at most
    // kMaxAttempts draws; then E9001).
    Result<std::string> generate(const ExistsCheck& exists);

    // 10 digits, first digit 1-9.
    static bool isValidPnr(const std::string& pnr);

private:
    std::string draw();
    std::mt19937_64 engine_;
    RandomSource random_;
};

}  // namespace rrs
