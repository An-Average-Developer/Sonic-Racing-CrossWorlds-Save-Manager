#include "Services/SaveFile/SaveCrypto.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <vector>

namespace SonicRacingSaveManager::Services::SaveCrypto {

namespace {

constexpr unsigned char kKey[32] = { 0xCA, 0x2C, 0x39, 0x8B, 0x76, 0xDA, 0x82, 0xAF, 0xE3, 0x6D, 0x17, 0x11, 0xA2, 0xD5, 0x97, 0xC1,
                                     0x7C, 0x74, 0x9E, 0x1F, 0x01, 0xFA, 0xFD, 0x4A, 0xDB, 0x7D, 0x94, 0x63, 0x82, 0xF7, 0xCC, 0xD3 };
constexpr size_t kBlock = 16;

// An AES-256-ECB key handle (one per call: cheap next to the file IO)
class Key {
public:
    Key() {
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("AES unavailable");
        BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
                          sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
        if (BCryptGenerateSymmetricKey(alg, &key, nullptr, 0, const_cast<PUCHAR>(kKey), sizeof(kKey), 0) < 0) throw std::runtime_error("AES key");
    }
    ~Key() {
        if (key) BCryptDestroyKey(key);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    }
    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;
    // In place, whole blocks, no padding
    void Run(std::string& data, bool encrypt) {
        ULONG done = 0;
        auto* p = reinterpret_cast<PUCHAR>(data.data());
        const auto n = static_cast<ULONG>(data.size());
        const NTSTATUS st = encrypt ? BCryptEncrypt(key, p, n, nullptr, nullptr, 0, p, n, &done, 0)
                                    : BCryptDecrypt(key, p, n, nullptr, nullptr, 0, p, n, &done, 0);
        if (st < 0 || done != n) throw std::runtime_error("AES failed");
    }

private:
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
};

} // namespace

std::optional<std::string> Decrypt(const std::string& encrypted) {
    if (encrypted.empty() || encrypted.size() % kBlock) return std::nullopt;
    std::string plain = encrypted;
    try {
        Key().Run(plain, false);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    const auto pad = static_cast<unsigned char>(plain.back());
    if (pad == 0 || pad > kBlock || pad > plain.size()) return std::nullopt;
    for (size_t k = plain.size() - pad; k < plain.size(); ++k)
        if (static_cast<unsigned char>(plain[k]) != pad) return std::nullopt;
    plain.resize(plain.size() - pad);
    if (plain.compare(0, 4, "GVAS") != 0) return std::nullopt;
    return plain;
}

std::string Encrypt(const std::string& plain) {
    std::string data = plain;
    const size_t pad = kBlock - data.size() % kBlock;
    data.append(pad, static_cast<char>(pad));
    Key().Run(data, true);
    return data;
}

} // namespace SonicRacingSaveManager::Services::SaveCrypto
