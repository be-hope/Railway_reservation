// pnr_generator.cpp

#include "booking/pnr_generator.h"

#include <cctype>

namespace rrs {

PnrGenerator::PnrGenerator() : engine_(std::random_device{}()) {
    random_ = [this]() { return static_cast<std::uint64_t>(engine_()); };
}

std::string PnrGenerator::draw() {
    const std::uint64_t r = random_();
    // First digit 1-9, then 9 more digits: 1,000,000,000 .. 9,999,999,999.
    const std::uint64_t value = 1000000000ULL + r % 9000000000ULL;
    return std::to_string(value);
}

Result<std::string> PnrGenerator::generate(const ExistsCheck& exists) {
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        std::string pnr = draw();
        Result<bool> taken = exists(pnr);
        if (!taken.ok()) return taken.error();
        if (!taken.value()) return pnr;
    }
    return Result<std::string>::failure(ErrorCode::Database, "could not draw a unique PNR");
}

bool PnrGenerator::isValidPnr(const std::string& pnr) {
    if (pnr.size() != 10 || pnr[0] < '1' || pnr[0] > '9') return false;
    for (char c : pnr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

}  // namespace rrs
