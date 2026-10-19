#include "Services/Platforms/SteamShortcutService.h"
#include <windows.h>
#include <ctime>
#include <regex>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"

namespace SonicRacingSaveManager::Services::SteamShortcuts {

VdfValue* VdfNode::Find(const std::string& key) {
    for (auto& [k, v] : items)
        if (k == key) return &v;
    return nullptr;
}

void VdfNode::Set(const std::string& key, VdfValue value) {
    if (VdfValue* existing = Find(key)) *existing = std::move(value);
    else items.emplace_back(key, std::move(value));
}

std::optional<std::wstring> FindSteamPath() {
    if (auto p = System::RegistryString(System::Hive::CurrentUser, L"Software\\Valve\\Steam", L"SteamPath");
        p && Files::DirectoryExists(*p))
        return Text::Replace(*p, L"/", L"\\");
    for (const wchar_t* key : { L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"SOFTWARE\\Valve\\Steam" }) {
        if (auto p = System::RegistryString(System::Hive::LocalMachine, key, L"InstallPath"); p && Files::DirectoryExists(*p)) return p;
    }
    for (const wchar_t* fallback : { L"C:\\Program Files (x86)\\Steam", L"C:\\Program Files\\Steam" })
        if (Files::Exists(Files::Combine(fallback, L"steam.exe"))) return std::wstring(fallback);
    return std::nullopt;
}

std::optional<std::wstring> FindActiveUserId(const std::wstring& steamPath) {
    const auto exists = [&](const std::wstring& id) { return Files::DirectoryExists(Files::Combine(steamPath, L"userdata", id)); };
    if (auto active = System::RegistryDword(System::Hive::CurrentUser, L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser");
        active && *active != 0 && exists(std::to_wstring(*active)))
        return std::to_wstring(*active);

    if (auto text = Files::ReadText(Files::Combine(steamPath, L"config", L"loginusers.vdf"))) {
        static const std::wregex userRe(LR"re("(7656\d{13})"\s*\{([^{}]*)\})re");
        static const std::wregex mostRecent(LR"re("MostRecent"\s*"1")re");
        static const std::wregex timestampRe(LR"re("Timestamp"\s*"(\d+)")re");
        std::optional<std::wstring> bestId;
        long long bestTimestamp = -1;
        for (std::wsregex_iterator it(text->begin(), text->end(), userRe), end; it != end; ++it) {
            const unsigned long long id64 = Text::ToUInt64((*it)[1].str()).value_or(0);
            const std::wstring block = (*it)[2].str();
            const std::wstring accountId = std::to_wstring(id64 - 76561197960265728ULL);
            if (std::regex_search(block, mostRecent)) {
                if (exists(accountId)) return accountId;
                return std::nullopt;
            }
            std::wsmatch m;
            const long long ts = std::regex_search(block, m, timestampRe) ? Text::ToInt(m[1].str()).value_or(0) : 0;
            if (ts > bestTimestamp) {
                bestTimestamp = ts;
                bestId = accountId;
            }
        }
        if (bestId && exists(*bestId)) return bestId;
    }

    std::vector<std::wstring> folders;
    for (const auto& dir : Files::ListDirectories(Files::Combine(steamPath, L"userdata")))
        if (Files::FileName(dir) != L"0") folders.push_back(Files::FileName(dir));
    if (folders.size() == 1) return folders[0];
    return std::nullopt;
}

namespace {

bool ParseObject(const std::string& data, size_t& pos, VdfNode& node, int depth) {
    if (depth > 64) return false;
    const auto cstring = [&](std::string& out) {
        const size_t end = data.find('\0', pos);
        if (end == std::string::npos) return false;
        out = data.substr(pos, end - pos);
        pos = end + 1;
        return true;
    };
    while (pos < data.size()) {
        const uint8_t type = static_cast<uint8_t>(data[pos++]);
        if (type == 0x08) return true;
        std::string key;
        if (!cstring(key)) return false;
        if (type == 0x00) {
            auto child = std::make_shared<VdfNode>();
            if (!ParseObject(data, pos, *child, depth + 1)) return false;
            node.items.emplace_back(key, child);
        } else if (type == 0x01) {
            std::string value;
            if (!cstring(value)) return false;
            node.items.emplace_back(key, value);
        } else if (type == 0x02) {
            if (pos + 4 > data.size()) return false;
            int32_t v = 0;
            memcpy(&v, data.data() + pos, 4);
            pos += 4;
            node.items.emplace_back(key, static_cast<int>(v));
        } else {
            return false;   // unknown type byte
        }
    }
    return false;
}

void WriteObject(std::string& out, const VdfNode& node) {
    for (const auto& [key, value] : node.items) {
        if (auto child = std::get_if<std::shared_ptr<VdfNode>>(&value)) {
            out += '\x00';
            out += key;
            out += '\0';
            WriteObject(out, **child);
        } else if (auto s = std::get_if<std::string>(&value)) {
            out += '\x01';
            out += key;
            out += '\0';
            out += *s;
            out += '\0';
        } else if (auto i = std::get_if<int>(&value)) {
            out += '\x02';
            out += key;
            out += '\0';
            out.append(reinterpret_cast<const char*>(i), 4);
        }
    }
    out += '\x08';
}

void AddOrUpdateEntry(VdfNode& root, const std::wstring& exePath, const std::wstring& startDir, const std::wstring& appNameW,
                      const std::wstring& iconPath) {
    const std::string appName = Text::Narrow(appNameW);
    VdfValue* existing = root.Find("shortcuts");
    if (!existing || !std::holds_alternative<std::shared_ptr<VdfNode>>(*existing)) {
        root.Set("shortcuts", std::make_shared<VdfNode>());
        existing = root.Find("shortcuts");
    }
    auto shortcuts = std::get<std::shared_ptr<VdfNode>>(*existing);

    std::vector<std::shared_ptr<VdfNode>> entries;
    for (auto& [k, v] : shortcuts->items)
        if (auto n = std::get_if<std::shared_ptr<VdfNode>>(&v)) entries.push_back(*n);
    std::shared_ptr<VdfNode> entry;
    for (auto& e : entries) {
        VdfValue* name = e->Find("AppName");
        if (name && std::holds_alternative<std::string>(*name) && std::get<std::string>(*name) == appName) entry = e;
    }
    const bool isNew = !entry;
    if (!entry) entry = std::make_shared<VdfNode>();

    const auto keep = [&](const char* key, VdfValue fallback) {
        VdfValue* v = entry->Find(key);
        entry->Set(key, v ? *v : fallback);
    };
    entry->Set("appid", static_cast<int>(ComputeAppId(exePath, appNameW)));
    entry->Set("AppName", appName);
    entry->Set("Exe", Text::Narrow(L"\"" + exePath + L"\""));
    entry->Set("StartDir", Text::Narrow(startDir));
    entry->Set("icon", Text::Narrow(iconPath));
    keep("ShortcutPath", std::string());
    keep("LaunchOptions", std::string());
    keep("IsHidden", 0);
    entry->Set("AllowDesktopConfig", 1);
    entry->Set("AllowOverlay", 1);
    keep("OpenVR", 0);
    keep("Devkit", 0);
    keep("DevkitGameID", std::string());
    keep("DevkitOverrideAppID", 0);
    entry->Set("LastPlayTime", static_cast<int>(std::time(nullptr)));
    keep("FlatpakAppID", std::string());
    keep("tags", std::make_shared<VdfNode>());
    if (isNew) entries.push_back(entry);

    // Steam wants the entries keyed "0", "1", "2"... in order
    shortcuts->items.clear();
    for (size_t i = 0; i < entries.size(); ++i) shortcuts->items.emplace_back(std::to_string(i), entries[i]);
}

int CopyIfExists(const std::wstring& from, const std::wstring& to) {
    if (!Files::Exists(from)) return 0;
    return Files::Copy(from, to, true) ? 1 : 0;
}

} // namespace

bool ReadVdf(const std::wstring& path, VdfNode& root) {
    auto data = Files::ReadBytes(path);
    if (!data) return false;
    size_t pos = 0;
    return ParseObject(*data, pos, root, 0);
}

bool WriteVdf(const std::wstring& path, const VdfNode& root) {
    std::string out;
    WriteObject(out, root);
    return Files::WriteBytes(path, out);
}

uint32_t ComputeAppId(const std::wstring& exePath, const std::wstring& appName) {
    const std::string s = Text::Narrow(L"\"" + exePath + L"\"" + appName);
    return Zip::Crc32(s.data(), s.size()) | 0x80000000u;
}

bool IsSteamRunning() { return !Process::FindByName(L"steam").empty(); }

void CloseSteam(const std::wstring& steamPath) {
    if (!IsSteamRunning()) return;
    Process::Start(Files::Combine(steamPath, L"steam.exe"), L"-shutdown");
    const ULONGLONG deadline = GetTickCount64() + 20000;
    while (GetTickCount64() < deadline && IsSteamRunning()) Sleep(500);
    for (const wchar_t* name : { L"steam", L"steamwebhelper", L"steamservice" })
        for (uint32_t pid : Process::FindByName(name)) Process::Kill(pid);
    Sleep(500);
}

void StartSteam(const std::wstring& steamPath) { Process::Start(Files::Combine(steamPath, L"steam.exe")); }

std::wstring AddOrUpdateShortcut(const std::wstring& exePath, const std::wstring& iconSourcePath) {
    auto steamPath = FindSteamPath();
    if (!steamPath) return L"Steam installation not found.";
    auto userId = FindActiveUserId(*steamPath);
    if (!userId) return L"Could not determine the active Steam user.";

    const std::wstring vdfPath = Files::Combine(*steamPath, L"userdata", *userId, L"config", L"shortcuts.vdf");
    // Icon: copy the chosen PNG somewhere permanent
    const std::wstring iconDir = Paths::DataFile(L"SteamIcon");
    const std::wstring iconPath = Files::Combine(iconDir, Files::FileName(iconSourcePath));
    Files::Copy(iconSourcePath, iconPath, true);
    const std::wstring startDir = Files::DirectoryName(exePath) + L"\\";

    VdfNode root;
    if (Files::Exists(vdfPath)) {
        Files::Copy(vdfPath, vdfPath + L".bak", true);
        if (!ReadVdf(vdfPath, root)) return L"Could not read shortcuts.vdf.";
    } else {
        Files::CreateDirectories(Files::DirectoryName(vdfPath));
    }
    AddOrUpdateEntry(root, exePath, startDir, AppName, iconPath);
    if (!WriteVdf(vdfPath, root)) return L"Could not write shortcuts.vdf.";

    // ApplyArtwork — library artwork: <appid>p.jpg poster, <appid>.jpg capsule, <appid>_hero.jpg banner, <appid>_logo.png logo
    const uint32_t appId = ComputeAppId(exePath, AppName);
    const std::wstring assets = Paths::Asset(L"Steam");
    const std::wstring grid = Files::Combine(*steamPath, L"userdata", *userId, L"config", L"grid");
    Files::CreateDirectories(grid);
    const std::wstring id = std::to_wstring(appId);
    int copied = 0;
    copied += CopyIfExists(Files::Combine(assets, L"Poster.jpg"), Files::Combine(grid, id + L"p.jpg"));
    copied += CopyIfExists(Files::Combine(assets, L"Capsule.jpg"), Files::Combine(grid, id + L".jpg"));
    copied += CopyIfExists(Files::Combine(assets, L"Banner.jpg"), Files::Combine(grid, id + L"_hero.jpg"));
    copied += CopyIfExists(Files::Combine(assets, L"Logo.png"), Files::Combine(grid, id + L"_logo.png"));
    if (copied == 0) return L"Shortcut was added, but no library artwork was found to copy from '" + assets + L"'.";
    return {};
}

} // namespace SonicRacingSaveManager::Services::SteamShortcuts
