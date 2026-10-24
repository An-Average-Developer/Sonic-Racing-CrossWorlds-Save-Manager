#include "Services/Backup/SteamAvatars.h"
#include <cwctype>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Text.h"
#include "Services/Platforms/SteamShortcutService.h"

namespace SonicRacingSaveManager::Services::SteamAvatars {

namespace {

// A SteamID64: 17 digits starting with 7656119
bool IsSteamId(const std::wstring& id) {
    if (id.size() != 17 || id.compare(0, 7, L"7656119") != 0) return false;
    for (wchar_t c : id)
        if (!std::iswdigit(c)) return false;
    return true;
}

// <avatarFull><![CDATA[https://avatars...._full.jpg]]></avatarFull>
std::string AvatarUrl(const std::string& xml) {
    const std::string open = "<avatarFull><![CDATA[";
    const size_t start = xml.find(open);
    if (start == std::string::npos) return {};
    const size_t end = xml.find("]]>", start + open.size());
    if (end == std::string::npos) return {};
    std::string url = xml.substr(start + open.size(), end - start - open.size());
    return url.compare(0, 8, "https://") == 0 ? url : std::string();
}

} // namespace

std::string Fetch(const std::wstring& steamId64) {
    if (!IsSteamId(steamId64)) return {};
    // Steam's cache
    if (const auto steam = SteamShortcuts::FindSteamPath()) {
        if (auto bytes = Files::ReadBytes(Files::Combine(*steam, L"config", L"avatarcache", steamId64 + L".png")); bytes && !bytes->empty())
            return std::move(*bytes);
    }
    // The public profile, then what was downloaded last time
    const std::wstring dir = Files::Combine(Paths::DataDir(), L"SteamAvatars"), file = Files::Combine(dir, steamId64 + L".jpg");
    const Http::Response profile = Http::Get(L"https://steamcommunity.com/profiles/" + steamId64 + L"?xml=1", {}, 10000);
    if (profile.Ok()) {
        if (const std::string url = AvatarUrl(profile.body); !url.empty()) {
            const Http::Response image = Http::Get(Text::Widen(url), {}, 15000);
            if (image.Ok() && !image.body.empty()) {
                if (Files::CreateDirectories(dir)) Files::WriteBytes(file, image.body);
                return image.body;
            }
        }
    }
    if (auto cached = Files::ReadBytes(file); cached && !cached->empty()) return std::move(*cached);
    return {};
}

} // namespace SonicRacingSaveManager::Services::SteamAvatars
