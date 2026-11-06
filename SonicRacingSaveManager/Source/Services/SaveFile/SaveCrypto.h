#pragma once
#include <optional>
#include <string>

// The .sav files are AES-256-ECB with PKCS#7 padding over the GVAS bytes. The key is the same for
// every account (taken from the running game's memory; it isn't in the exe as is, Denuvo hides it).
namespace SonicRacingSaveManager::Services::SaveCrypto {

// The GVAS bytes of an encrypted save; nullopt when it doesn't decrypt to one
std::optional<std::string> Decrypt(const std::string& encrypted);
std::string Encrypt(const std::string& plain);

} // namespace SonicRacingSaveManager::Services::SaveCrypto
