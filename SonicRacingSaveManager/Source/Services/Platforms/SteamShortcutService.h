#pragma once
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Adds/updates the manager's own entry in Steam's "Add a Non-Steam Game" list
// (userdata\<id>\config\shortcuts.vdf, Valve's binary VDF format) plus its library artwork.
namespace SonicRacingSaveManager::Services::SteamShortcuts {

inline constexpr const wchar_t* AppName = L"Sonic Racing CrossWorlds Save Manager";

// Binary VDF node (insertion order preserved)
struct VdfNode;
using VdfValue = std::variant<std::shared_ptr<VdfNode>, std::string, int>;
struct VdfNode {
    std::vector<std::pair<std::string, VdfValue>> items;
    VdfValue* Find(const std::string& key);
    void Set(const std::string& key, VdfValue value);
};

std::optional<std::wstring> FindSteamPath();
std::optional<std::wstring> FindActiveUserId(const std::wstring& steamPath);
bool ReadVdf(const std::wstring& path, VdfNode& root);
bool WriteVdf(const std::wstring& path, const VdfNode& root);
uint32_t ComputeAppId(const std::wstring& exePath, const std::wstring& appName);

bool IsSteamRunning();
void CloseSteam(const std::wstring& steamPath);   // graceful -shutdown, then kills stragglers
void StartSteam(const std::wstring& steamPath);

// Steam must be closed already. Returns an error message, or empty on success.
std::wstring AddOrUpdateShortcut(const std::wstring& exePath, const std::wstring& iconSourcePath);

} // namespace SonicRacingSaveManager::Services::SteamShortcuts
