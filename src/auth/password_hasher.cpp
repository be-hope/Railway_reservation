#include "auth/password_hasher.h"
#include <array>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace rrs {
namespace {
std::string hex(const unsigned char* bytes, std::size_t n) {
    constexpr char alphabet[] = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < n; ++i) { out += alphabet[bytes[i] >> 4]; out += alphabet[bytes[i] & 15]; }
    return out;
}
bool decodeSalt(const std::string& s, std::array<unsigned char, 16>& out) {
    if (s.size() != 32) return false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned int byte = 0;
        for (std::size_t j = 0; j < 2; ++j) {
            char c = s[i * 2 + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
            byte = byte * 16 + (c <= '9' ? c - '0' : c - 'a' + 10);
        }
        out[i] = static_cast<unsigned char>(byte);
    }
    return true;
}
std::string derive(const std::string& password, const unsigned char* salt) {
    std::array<unsigned char, 32> bytes{};
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt, 16,
                          PasswordHasher::kIterations, EVP_sha256(), static_cast<int>(bytes.size()), bytes.data()) != 1) return {};
    return hex(bytes.data(), bytes.size());
}
}
Result<std::string> PasswordHasher::randomToken() {
    std::array<unsigned char, 16> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) return Result<std::string>::failure(ErrorCode::Internal, "Secure random generation failed.");
    return hex(bytes.data(), bytes.size());
}
Result<PasswordDigest> PasswordHasher::hash(const std::string& password) {
    if (password.empty() || password.size() > 128) return Result<PasswordDigest>::failure(ErrorCode::Validation);
    auto salt = randomToken();
    if (!salt.ok()) return salt.error();
    std::array<unsigned char, 16> bytes{};
    if (!decodeSalt(salt.value(), bytes)) return Result<PasswordDigest>::failure(ErrorCode::Internal);
    auto hash = derive(password, bytes.data());
    if (hash.empty()) return Result<PasswordDigest>::failure(ErrorCode::Internal);
    return PasswordDigest{hash, salt.value()};
}
bool PasswordHasher::verify(const std::string& password, const std::string& hash, const std::string& salt) {
    if (password.size() > 128) return false;
    std::array<unsigned char, 16> bytes{};
    const bool valid = decodeSalt(salt, bytes) && hash.size() == 64;
    const std::string candidate = derive(password, bytes.data()); // dummy work even for disabled/malformed credentials
    return valid && !candidate.empty() && CRYPTO_memcmp(candidate.data(), hash.data(), 64) == 0;
}
}
