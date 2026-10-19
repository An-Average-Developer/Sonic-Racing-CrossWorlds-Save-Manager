#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Models/ModInfo.h"

// The GameBanana half of ModManagerService.cs: apiv11 requests with the C#'s browser-like headers
// (10 s timeout). Blocking: call from worker threads.
namespace SonicRacingSaveManager::Services::Mods::GameBanana {

struct UpdateCheck {
    std::wstring latestVersion;
    bool hasUpdate = false;
};

// CheckForUpdateAsync: the item's _sVersion and whether it's newer than `localVersion`. nullopt when
// there's no usable URL, the request fails (403 included) or there's no version — the C# left the
// mod untouched then.
std::optional<UpdateCheck> CheckForUpdate(const std::wstring& modPageUrl, const std::wstring& localVersion);

// GetAvailableFilesAsync: the item's _aFiles. nullopt when the URL isn't a GameBanana item or the
// response has no files; throws Mods::Error when the request fails (EnsureSuccessStatusCode)
std::optional<std::vector<Models::GameBananaFile>> GetAvailableFiles(const std::wstring& modPageUrl);

// IsNewerVersion: every number of the versions (anything but digits and dots removed), the shorter
// one padded with zeros
bool IsNewerVersion(const std::wstring& local, const std::wstring& remote);

// The request headers (download requests use them too)
const std::vector<std::pair<std::wstring, std::wstring>>& Headers();

} // namespace SonicRacingSaveManager::Services::Mods::GameBanana
