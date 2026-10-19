#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace SonicRacingSaveManager::Services::ResourceExtractor {

// Unpacks the assets embedded in the exe to %TEMP%\SonicRacingSaveManager (the folder
// SteamAssetExtractor used). Unchanged files from
// a previous run are kept. `progress` gets (done, total) after every asset; any thread.
void Extract(const std::function<void(size_t done, size_t total)>& progress = {});
// Bytes of one embedded asset ("Assets", "Steam\\Logo.png"), without extracting anything.
std::optional<std::string> ReadEmbedded(const std::wstring& root, const std::wstring& path);

} // namespace SonicRacingSaveManager::Services::ResourceExtractor
