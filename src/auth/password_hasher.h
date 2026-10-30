#pragma once
#include "common/result.h"
namespace rrs {
struct PasswordDigest { std::string hash, salt; };
// Fixed work factor is a storage contract; changing it requires a hash migration.
class PasswordHasher {
public:
    static constexpr int kIterations = 600000;
    static Result<PasswordDigest> hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& hash, const std::string& salt);
    static Result<std::string> randomToken();
};
}
