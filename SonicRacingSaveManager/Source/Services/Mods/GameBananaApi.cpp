#include "Services/Mods/GameBananaApi.h"
#include <algorithm>
#include <climits>
#include <cwctype>
#include "App/Config.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Mods/ModManager.h"

namespace SonicRacingSaveManager::Services::Mods::GameBanana {

namespace {

namespace C = Config::GameBanana;

// _gb403ErrorShown: the 403 note is logged once per session
bool gb403ErrorShown = false;

// ExtractGameBananaItemDetails: ".../gamebanana.com/<type>/<id>..." → (type, id)
std::optional<std::pair<std::wstring, std::wstring>> ItemDetails(const std::wstring& url) {
    if (url.find(L"gamebanana.com/") == std::wstring::npos) return std::nullopt;
    const auto parts = Text::Split(url, L'/');
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i + 1 < parts.size() && parts[i] == L"gamebanana.com") {
            std::wstring itemId;
            if (i + 2 < parts.size()) {
                itemId = parts[i + 2];
                while (!itemId.empty() && itemId.back() == L'/') itemId.pop_back();
            }
            if (parts[i + 1].empty() || itemId.empty()) return std::nullopt;
            return std::make_pair(parts[i + 1], itemId);
        }
    }
    return std::nullopt;
}

// "mods" → "Mod" (TrimEnd('s') + capital first letter), then the apiv11 URL
std::optional<std::wstring> ApiUrl(const std::wstring& modPageUrl, const wchar_t* properties) {
    const auto details = ItemDetails(modPageUrl);
    if (!details) return std::nullopt;
    std::wstring type = details->first;
    while (!type.empty() && type.back() == L's') type.pop_back();
    if (!type.empty()) type[0] = static_cast<wchar_t>(std::towupper(type[0]));
    return std::wstring(C::ApiUrl) + type + L"/" + details->second + L"?_csvProperties=" + properties;
}

// HttpRequestException's message for a failed status
std::wstring StatusError(const Http::Response& r) {
    if (r.status == 0) return r.error.empty() ? Loc::Get("mods_error_network") : r.error;
    return Loc::Fmt("mods_error_status", r.status);
}

std::vector<int> VersionToNumbers(const std::wstring& version) {
    std::wstring cleaned;
    for (wchar_t c : version)
        if ((c >= L'0' && c <= L'9') || c == L'.') cleaned += c;
    std::vector<int> parts;
    for (const auto& p : Text::Split(cleaned, L'.')) {
        if (p.empty()) continue;
        const auto n = Text::ToInt(p);
        if (n && *n >= INT_MIN && *n <= INT_MAX) parts.push_back(static_cast<int>(*n));   // int.TryParse
    }
    if (parts.empty()) parts.push_back(0);
    return parts;
}

} // namespace

const std::vector<std::pair<std::wstring, std::wstring>>& Headers() {
    static const std::vector<std::pair<std::wstring, std::wstring>> headers = {
        { L"User-Agent", C::UserAgent }, { L"Accept", C::Accept }, { L"Accept-Language", C::AcceptLanguage },
        { L"Referer", C::Referer },       { L"Origin", C::Origin },
    };
    return headers;
}

bool IsNewerVersion(const std::wstring& local, const std::wstring& remote) {
    auto localNums = VersionToNumbers(local);
    auto remoteNums = VersionToNumbers(remote);
    const size_t maxLength = std::max(localNums.size(), remoteNums.size());
    localNums.resize(maxLength, 0);
    remoteNums.resize(maxLength, 0);
    for (size_t i = 0; i < maxLength; ++i) {
        if (remoteNums[i] > localNums[i]) return true;
        if (remoteNums[i] < localNums[i]) return false;
    }
    return false;
}

std::optional<UpdateCheck> CheckForUpdate(const std::wstring& modPageUrl, const std::wstring& localVersion) {
    if (modPageUrl.empty()) return std::nullopt;
    const auto url = ApiUrl(modPageUrl, L"_sVersion");
    if (!url) return std::nullopt;
    const Http::Response r = Http::Get(*url, Headers(), C::TimeoutMs);
    if (r.status == 403) {
        if (!gb403ErrorShown) {
            gb403ErrorShown = true;
            Log::Warning(L"GameBanana's API returned a 403 Forbidden error. This usually means their servers are temporarily blocking requests.");
        }
        return std::nullopt;
    }
    if (!r.Ok()) return std::nullopt;
    bool ok = false;
    const Json::Value json = Json::Parse(std::string_view(r.body), &ok);
    if (!ok || !json["_sVersion"].IsString()) return std::nullopt;
    UpdateCheck check;
    check.latestVersion = json["_sVersion"].WStr();
    if (check.latestVersion.empty()) return std::nullopt;
    if (!localVersion.empty()) check.hasUpdate = IsNewerVersion(localVersion, check.latestVersion);
    return check;
}

std::optional<std::vector<Models::GameBananaFile>> GetAvailableFiles(const std::wstring& modPageUrl) {
    if (modPageUrl.empty()) return std::nullopt;
    const auto url = ApiUrl(modPageUrl, L"_aFiles,_sVersion");
    if (!url) return std::nullopt;
    const Http::Response r = Http::Get(*url, Headers(), C::TimeoutMs);
    if (!r.Ok()) throw Error(StatusError(r));
    bool ok = false;
    const Json::Value json = Json::Parse(std::string_view(r.body), &ok);
    if (!ok) throw Error(Loc::Get("mods_error_bad_response"));
    if (!json["_aFiles"].IsArray()) return std::nullopt;
    std::vector<Models::GameBananaFile> files;
    for (const auto& f : json["_aFiles"].Elements()) {
        Models::GameBananaFile file;
        if (f["_sFile"].IsString()) file.fileName = f["_sFile"].WStr();
        if (f["_sDownloadUrl"].IsString()) file.downloadUrl = f["_sDownloadUrl"].WStr();
        if (f["_nFilesize"].IsNumber()) file.fileSize = f["_nFilesize"].Int();
        files.push_back(std::move(file));
    }
    return files;
}

} // namespace SonicRacingSaveManager::Services::Mods::GameBanana
